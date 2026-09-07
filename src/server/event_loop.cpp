#include "netpulse/server/event_loop.hpp"

#include "netpulse/network/nonblocking.hpp"
#include "netpulse/protocol/frame_write_buffer.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <span>
#include <string_view>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <utility>

namespace netpulse::server {

namespace {

constexpr std::size_t kMaxEvents{64};

constexpr std::uint32_t kClientEvents{
    EPOLLIN | EPOLLRDHUP
};

constexpr std::string_view kResponse{
    "ACK from NetPulse!"
};

std::span<const std::byte> asBytes(
    std::string_view text)
{
    return std::as_bytes(
        std::span{
            text.data(),
            text.size()
        });
}

}  // namespace

EventLoop::EventLoop(
    netpulse::network::Socket listener) noexcept
    : listener_{std::move(listener)}
{
    using netpulse::network::setNonBlocking;

    if (!listener_.valid()) {
        error_number_ = EBADF;
        return;
    }

    if (!epoll_.valid()) {
        error_number_ = errno;
        return;
    }

    const auto nonblocking_result{
        setNonBlocking(listener_.fd())
    };

    if (!nonblocking_result) {
        error_number_ =
            nonblocking_result.error_number;

        return;
    }

    const auto add_result{
        epoll_.add(listener_.fd(), EPOLLIN)
    };

    if (!add_result) {
        error_number_ = add_result.error_number;
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
    if (!valid()) {
        return 1;
    }

    std::array<epoll_event, kMaxEvents> events{};

    while (true) {
        const auto wait_result{
            epoll_.wait(
                std::span<epoll_event>{events},
                -1)
        };

        if (!wait_result.completed()) {
            error_number_ =
                wait_result.error_number;

            std::cerr
                << "epoll_wait() failed: "
                << std::strerror(error_number_)
                << '\n';

            return 1;
        }

        for (int index = 0;
             index < wait_result.event_count;
             ++index) {
            const epoll_event& event{
                events[static_cast<std::size_t>(
                    index)]
            };

            const int ready_fd{event.data.fd};

            if (ready_fd == listener_.fd()) {
                if ((event.events & EPOLLIN) != 0) {
                    if (!acceptReadyClients()) {
                        return 1;
                    }
                }

                if ((event.events
                     & (EPOLLERR | EPOLLHUP))
                    != 0) {
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
    while (true) {
        const int client_fd = ::accept4(
            listener_.fd(),
            nullptr,
            nullptr,
            SOCK_NONBLOCK | SOCK_CLOEXEC);

        if (client_fd >= 0) {
            netpulse::network::Socket client_socket{
                client_fd
            };

            auto [iterator, inserted] =
                connections_.try_emplace(
                    client_fd,
                    std::move(client_socket));

            if (!inserted) {
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

            if (!add_result) {
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
                << ", active="
                << connections_.size()
                << '\n';

            continue;
        }

        const int error_number{errno};

        if (error_number == EINTR) {
            continue;
        }

        if (error_number == EAGAIN
            || error_number == EWOULDBLOCK) {
            return true;
        }

        error_number_ = error_number;

        std::cerr
            << "accept4() failed: "
            << std::strerror(error_number_)
            << '\n';

        return false;
    }
}

void EventLoop::handleClientEvent(
    int client_fd,
    std::uint32_t events)
{
    using netpulse::protocol::FrameFlushStatus;
    using netpulse::protocol::FrameQueueStatus;

    const auto iterator{
        connections_.find(client_fd)
    };

    if (iterator == connections_.end()) {
        return;
    }

    Connection& connection{iterator->second};

    if ((events & (EPOLLERR | EPOLLHUP)) != 0) {
        std::cerr
            << "Client socket failed, fd="
            << client_fd
            << '\n';

        closeConnection(client_fd);
        return;
    }

    if ((events & (EPOLLIN | EPOLLRDHUP)) != 0) {
        auto read_result{
            connection.readAvailable()
        };

        for (const auto& frame : read_result.frames) {
            std::cout
                << "Received from fd="
                << client_fd
                << ", bytes="
                << frame.size()
                << ": ";

            if (!frame.empty()) {
                std::cout.write(
                    reinterpret_cast<const char*>(
                        frame.data()),
                    static_cast<std::streamsize>(
                        frame.size()));
            }

            std::cout << '\n';

            const auto queue_status{
                connection.queueFrame(
                    asBytes(kResponse))
            };

            if (queue_status
                != FrameQueueStatus::queued) {
                std::cerr
                    << "Could not queue response for fd="
                    << client_fd
                    << '\n';

                closeConnection(client_fd);
                return;
            }
        }

        if (read_result.status
            == ConnectionReadStatus::peer_closed) {
            std::cout
                << "Client disconnected, fd="
                << client_fd
                << '\n';

            closeConnection(client_fd);
            return;
        }

        if (read_result.status
            == ConnectionReadStatus::protocol_error) {
            std::cerr
                << "Protocol error from fd="
                << client_fd
                << '\n';

            closeConnection(client_fd);
            return;
        }

        if (read_result.status
            == ConnectionReadStatus::io_error) {
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

    if (connection.wantsWrite()) {
        const auto flush_result{
            connection.flushWrites()
        };

        if (flush_result.status
            == FrameFlushStatus::error) {
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

    if (!updateInterest(connection)) {
        closeConnection(client_fd);
    }
}

bool EventLoop::updateInterest(
    Connection& connection)
{
    std::uint32_t events{kClientEvents};

    if (connection.wantsWrite()) {
        events |= EPOLLOUT;
    }

    const auto modify_result{
        epoll_.modify(
            connection.fd(),
            events)
    };

    if (!modify_result) {
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

    if (iterator == connections_.end()) {
        return;
    }

    const auto remove_result{
        epoll_.remove(client_fd)
    };

    if (!remove_result
        && remove_result.error_number != ENOENT
        && remove_result.error_number != EBADF) {
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
        << ", active="
        << connections_.size()
        << '\n';
}

}  // namespace netpulse::server