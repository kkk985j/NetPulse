#pragma once

#include "netpulse/network/epoll.hpp"
#include "netpulse/network/socket.hpp"
#include "netpulse/server/connection.hpp"
#include "netpulse/server/processing.hpp"
#include "netpulse/server/processing_dispatcher.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace netpulse::server {

class EventLoop
{
public:
    EventLoop(
        netpulse::network::Socket listener,
        std::size_t worker_count,
        std::size_t task_queue_capacity);

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
    struct ClientState
    {
        ClientState(
            ConnectionId connection_id,
            netpulse::network::Socket socket) noexcept
            : id{connection_id},
              connection{std::move(socket)}
        {
        }

        ConnectionId id{0};
        Connection connection;
    };

    [[nodiscard]] bool acceptReadyClients();

    void handleClientEvent(
        int client_fd,
        std::uint32_t events);

    [[nodiscard]] bool submitFrameForProcessing(
        int client_fd,
        ConnectionId connection_id,
        std::vector<std::byte> payload);

    [[nodiscard]] bool handleProcessingResults();

    [[nodiscard]] bool updateInterest(
        Connection& connection);

    void closeConnection(int client_fd);

    netpulse::network::Socket listener_{};
    netpulse::network::Epoll epoll_{};

    std::unordered_map<int, ClientState>
        connections_{};

    ConnectionId next_connection_id_{1};

    int error_number_{0};
    bool initialized_{false};

    // 放在最后，使析构时先停止后台工作线程。
    ProcessingDispatcher dispatcher_;
};

} // namespace netpulse::server