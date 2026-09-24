#include "netpulse/server/event_loop.hpp"

#include "netpulse/network/nonblocking.hpp"
#include "netpulse/protocol/frame_write_buffer.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <span>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <utility>

namespace netpulse::server {

namespace {

constexpr std::size_t kMaxEvents{64};

constexpr std::uint32_t kClientEvents{
    EPOLLIN | EPOLLRDHUP
};

} // namespace

EventLoop::EventLoop(
    netpulse::network::Socket listener,
    std::size_t worker_count,
    std::size_t task_queue_capacity)
    : listener_{std::move(listener)},
      dispatcher_{
          worker_count,
          task_queue_capacity}
{
    using netpulse::network::setNonBlocking;

    if (!listener_.valid())
    {
        error_number_ = EBADF;
        return;
    }

    if (!epoll_.valid())
    {
        error_number_ = errno;
        return;
    }

    if (!dispatcher_.valid())
    {
        error_number_ =
            dispatcher_.errorNumber();

        return;
    }

    const auto nonblocking_result{
        setNonBlocking(listener_.fd())
    };

    if (!nonblocking_result)
    {
        error_number_ =
            nonblocking_result.error_number;

        return;
    }

    const auto listener_add_result{
        epoll_.add(
            listener_.fd(),
            EPOLLIN)
    };

    if (!listener_add_result)
    {
        error_number_ =
            listener_add_result.error_number;

        return;
    }

    const auto dispatcher_add_result{
        epoll_.add(
            dispatcher_.notificationFd(),
            EPOLLIN)
    };

    if (!dispatcher_add_result)
    {
        error_number_ =
            dispatcher_add_result.error_number;

        return;
    }

    initialized_ = true;
}

bool EventLoop::valid() const noexcept
{
    return initialized_;
}

int EventLoop::errorNumber() const noexcept
{
    return error_number_;
}

int EventLoop::run()
{
    if (!valid())
    {
        return 1;
    }

    std::array<epoll_event, kMaxEvents> events{};

    while (true)
    {
        const auto wait_result{
            epoll_.wait(
                std::span<epoll_event>{events},
                -1)
        };

        if (!wait_result.completed())
        {
            error_number_ =
                wait_result.error_number;

            std::cerr
                << "epoll_wait() failed: "
                << std::strerror(error_number_)
                << '\n';

            return 1;
        }

        for (int index{0};
             index < wait_result.event_count;
             ++index)
        {
            const epoll_event& event{
                events[static_cast<std::size_t>(
                    index)]
            };

            const int ready_fd{
                event.data.fd
            };

            if (ready_fd ==
                dispatcher_.notificationFd())
            {
                if ((event.events &
                     (EPOLLERR | EPOLLHUP)) != 0)
                {
                    error_number_ = EIO;

                    std::cerr
                        << "Processing notifier "
                        << "failed.\n";

                    return 1;
                }

                if ((event.events & EPOLLIN) != 0)
                {
                    if (!handleProcessingResults())
                    {
                        return 1;
                    }
                }

                continue;
            }

            if (ready_fd == listener_.fd())
            {
                if ((event.events & EPOLLIN) != 0)
                {
                    if (!acceptReadyClients())
                    {
                        return 1;
                    }
                }

                if ((event.events &
                     (EPOLLERR | EPOLLHUP)) != 0)
                {
                    error_number_ = EIO;

                    std::cerr
                        << "Listening socket failed.\n";

                    return 1;
                }

                continue;
            }

            handleClientEvent(
                ready_fd,
                event.events);
        }
    }
}

std::size_t EventLoop::connectionCount()
    const noexcept
{
    return connections_.size();
}

bool EventLoop::acceptReadyClients()
{
    while (true)
    {
        const int client_fd{
            ::accept4(
                listener_.fd(),
                nullptr,
                nullptr,
                SOCK_NONBLOCK | SOCK_CLOEXEC)
        };

        if (client_fd >= 0)
        {
            netpulse::network::Socket client_socket{
                client_fd
            };

            const ConnectionId connection_id{
                next_connection_id_++
            };

            auto [iterator, inserted] =
                connections_.try_emplace(
                    client_fd,
                    connection_id,
                    std::move(client_socket));

            if (!inserted)
            {
                std::cerr
                    << "Duplicate client descriptor: "
                    << client_fd
                    << '\n';

                continue;
            }

            const auto add_result{
                epoll_.add(
                    client_fd,
                    kClientEvents)
            };

            if (!add_result)
            {
                std::cerr
                    << "Could not register client "
                    << client_fd
                    << ": "
                    << std::strerror(
                        add_result.error_number)
                    << '\n';

                connections_.erase(iterator);
                continue;
            }

            std::cout
                << "Client connected, fd="
                << client_fd
                << ", connection_id="
                << connection_id
                << ", active="
                << connections_.size()
                << '\n';

            continue;
        }

        const int operation_error{errno};

        if (operation_error == EINTR)
        {
            continue;
        }

        if (operation_error == EAGAIN ||
            operation_error == EWOULDBLOCK)
        {
            return true;
        }

        error_number_ = operation_error;

        std::cerr
            << "accept4() failed: "
            << std::strerror(error_number_)
            << '\n';

        return false;
    }
}

bool EventLoop::submitFrameForProcessing(
    int client_fd,
    ConnectionId connection_id,
    std::vector<std::byte> payload)
{
    using netpulse::concurrency::SubmitStatus;

    const auto status{
        dispatcher_.trySubmit(
            ProcessingTask{
                client_fd,
                connection_id,
                std::move(payload)})
    };

    switch (status)
    {
    case SubmitStatus::Success:
        return true;

    case SubmitStatus::QueueFull:
        std::cerr
            << "Processing queue full for fd="
            << client_fd
            << ", connection_id="
            << connection_id
            << '\n';

        return false;

    case SubmitStatus::Stopped:
        std::cerr
            << "Processing dispatcher stopped.\n";

        return false;

    case SubmitStatus::InvalidTask:
        std::cerr
            << "Invalid processing task for fd="
            << client_fd
            << '\n';

        return false;
    }

    return false;
}

bool EventLoop::handleProcessingResults()
{
    using netpulse::protocol::FrameQueueStatus;

    const auto notification{
        dispatcher_.consumeNotifications()
    };

    if (!notification.success)
    {
        if (notification.error_number == EAGAIN ||
            notification.error_number == EWOULDBLOCK)
        {
            return true;
        }

        error_number_ =
            notification.error_number;

        std::cerr
            << "Could not consume processing "
            << "notification: "
            << std::strerror(error_number_)
            << '\n';

        return false;
    }

    auto results{
        dispatcher_.takeCompleted()
    };

    for (auto& result : results)
    {
        const auto iterator{
            connections_.find(result.client_fd)
        };

        if (iterator == connections_.end())
        {
            continue;
        }

        ClientState& client_state{
            iterator->second
        };

        if (client_state.id !=
            result.connection_id)
        {
            std::cerr
                << "Discarding stale result for fd="
                << result.client_fd
                << ", result_connection_id="
                << result.connection_id
                << ", current_connection_id="
                << client_state.id
                << '\n';

            continue;
        }

        Connection& connection{
            client_state.connection
        };

        const auto queue_status{
            connection.queueFrame(
                std::span<const std::byte>{
                    result.response.data(),
                    result.response.size()})
        };

        if (queue_status !=
            FrameQueueStatus::queued)
        {
            std::cerr
                << "Could not queue processed "
                << "response for fd="
                << result.client_fd
                << '\n';

            closeConnection(result.client_fd);
            continue;
        }

        if (!updateInterest(connection))
        {
            closeConnection(result.client_fd);
        }
    }

    return true;
}

void EventLoop::handleClientEvent(
    int client_fd,
    std::uint32_t events)
{
    using netpulse::protocol::FrameFlushStatus;

    const auto iterator{
        connections_.find(client_fd)
    };

    if (iterator == connections_.end())
    {
        return;
    }

    ClientState& client_state{
        iterator->second
    };

    Connection& connection{
        client_state.connection
    };

    if ((events & (EPOLLERR | EPOLLHUP)) != 0)
    {
        std::cerr
            << "Client socket failed, fd="
            << client_fd
            << '\n';

        closeConnection(client_fd);
        return;
    }

    if ((events & (EPOLLIN | EPOLLRDHUP)) != 0)
    {
        auto read_result{
            connection.readAvailable()
        };

        for (auto& frame : read_result.frames)
        {
            std::cout
                << "Received from fd="
                << client_fd
                << ", connection_id="
                << client_state.id
                << ", bytes="
                << frame.size()
                << ": ";

            if (!frame.empty())
            {
                std::cout.write(
                    reinterpret_cast<const char*>(
                        frame.data()),
                    static_cast<std::streamsize>(
                        frame.size()));
            }

            std::cout << '\n';

            if (!submitFrameForProcessing(
                    client_fd,
                    client_state.id,
                    std::move(frame)))
            {
                closeConnection(client_fd);
                return;
            }
        }

        if (read_result.status ==
            ConnectionReadStatus::peer_closed)
        {
            std::cout
                << "Client disconnected, fd="
                << client_fd
                << '\n';

            closeConnection(client_fd);
            return;
        }

        if (read_result.status ==
            ConnectionReadStatus::protocol_error)
        {
            std::cerr
                << "Protocol error from fd="
                << client_fd
                << '\n';

            closeConnection(client_fd);
            return;
        }

        if (read_result.status ==
            ConnectionReadStatus::io_error)
        {
            std::cerr
                << "Read error from fd="
                << client_fd
                << ": "
                << std::strerror(
                    read_result.error_number)
                << '\n';

            closeConnection(client_fd);
            return;
        }
    }

    if (connection.wantsWrite())
    {
        const auto flush_result{
            connection.flushWrites()
        };

        if (flush_result.status ==
            FrameFlushStatus::error)
        {
            std::cerr
                << "Write error for fd="
                << client_fd
                << ": "
                << std::strerror(
                    flush_result.error_number)
                << '\n';

            closeConnection(client_fd);
            return;
        }
    }

    if (!updateInterest(connection))
    {
        closeConnection(client_fd);
    }
}

bool EventLoop::updateInterest(
    Connection& connection)
{
    std::uint32_t events{kClientEvents};

    if (connection.wantsWrite())
    {
        events |= EPOLLOUT;
    }

    const auto modify_result{
        epoll_.modify(
            connection.fd(),
            events)
    };

    if (!modify_result)
    {
        std::cerr
            << "Could not update epoll interest for fd="
            << connection.fd()
            << ": "
            << std::strerror(
                modify_result.error_number)
            << '\n';

        return false;
    }

    return true;
}

void EventLoop::closeConnection(int client_fd)
{
    const auto iterator{
        connections_.find(client_fd)
    };

    if (iterator == connections_.end())
    {
        return;
    }

    const ConnectionId connection_id{
        iterator->second.id
    };

    const auto remove_result{
        epoll_.remove(client_fd)
    };

    if (!remove_result &&
        remove_result.error_number != ENOENT &&
        remove_result.error_number != EBADF)
    {
        std::cerr
            << "Could not remove fd="
            << client_fd
            << " from epoll: "
            << std::strerror(
                remove_result.error_number)
            << '\n';
    }

    connections_.erase(iterator);

    std::cout
        << "Connection closed, fd="
        << client_fd
        << ", connection_id="
        << connection_id
        << ", active="
        << connections_.size()
        << '\n';
}

} // namespace netpulse::server