#pragma once

#include "netpulse/network/epoll.hpp"
#include "netpulse/network/socket.hpp"
#include "netpulse/server/connection.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace netpulse::server {

class EventLoop {
public:
    explicit EventLoop(
        netpulse::network::Socket listener) noexcept;

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    EventLoop(EventLoop&&) = delete;
    EventLoop& operator=(EventLoop&&) = delete;

    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] int errorNumber() const noexcept;

    int run();

    [[nodiscard]] std::size_t connectionCount()
        const noexcept;

private:
    [[nodiscard]] bool acceptReadyClients();

    void handleClientEvent(
        int client_fd,
        std::uint32_t events);

    [[nodiscard]] bool updateInterest(
        Connection& connection);

    void closeConnection(int client_fd);

    netpulse::network::Socket listener_{};
    netpulse::network::Epoll epoll_{};

    std::unordered_map<int, Connection>
        connections_{};

    int error_number_{0};
    bool initialized_{false};
};

}  // namespace netpulse::server