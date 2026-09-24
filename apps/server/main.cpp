#include "netpulse/network/socket.hpp"
#include "netpulse/server/event_loop.hpp"

#include <arpa/inet.h>
#include <cstddef>
#include <exception>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <utility>

using netpulse::network::Socket;
using netpulse::server::EventLoop;

namespace {

constexpr int kServerPort{9000};
constexpr int kListenBacklog{128};

constexpr std::size_t kWorkerCount{4};
constexpr std::size_t kTaskQueueCapacity{1024};

}  // namespace

int main()
{
    Socket listener{
        ::socket(
            AF_INET,
            SOCK_STREAM | SOCK_CLOEXEC,
            0)
    };

    if (!listener.valid()) {
        std::cerr
            << "Failed to create listening socket: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    constexpr int reuse_address{1};

    if (::setsockopt(
            listener.fd(),
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse_address,
            sizeof(reuse_address)) < 0) {
        std::cerr
            << "Failed to set SO_REUSEADDR: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr =
        htonl(INADDR_ANY);
    server_address.sin_port = htons(kServerPort);

    if (::bind(
            listener.fd(),
            reinterpret_cast<sockaddr*>(
                &server_address),
            sizeof(server_address)) < 0) {
        std::cerr
            << "Failed to bind listening socket: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    if (::listen(
            listener.fd(),
            kListenBacklog) < 0) {
        std::cerr
            << "Failed to listen: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    try
    {
        EventLoop event_loop{
            std::move(listener),
            kWorkerCount,
            kTaskQueueCapacity
        };

        if (!event_loop.valid())
        {
            std::cerr
                << "Failed to initialize event loop: "
                << std::strerror(
                    event_loop.errorNumber())
                << '\n';

            return 1;
        }

        std::cout
            << "NetPulse epoll server listening on "
            << "0.0.0.0:"
            << kServerPort
            << ", workers="
            << kWorkerCount
            << ", task_queue_capacity="
            << kTaskQueueCapacity
            << '\n';

        return event_loop.run();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Failed to create event loop: "
            << exception.what()
            << '\n';

        return 1;
    }
}