#pragma once

#include "netpulse/network/epoll.hpp"
#include "netpulse/network/socket.hpp"
#include "netpulse/server/connection.hpp"
#include "netpulse/server/processing.hpp"
#include "netpulse/server/processing_dispatcher.hpp"
#include "netpulse/storage/database_config.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace netpulse::server {

/// @brief 基于 epoll 的非阻塞服务端事件循环。
class EventLoop
{
public:
    /// @brief 创建事件循环和后台业务处理器。
    /// @param listener 已经完成 bind 和 listen 的监听 Socket。
    /// @param worker_count 后台工作线程数量。
    /// @param task_queue_capacity 最大等待任务数量。
    /// @param database_config 数据库连接配置。
    EventLoop(
        netpulse::network::Socket listener,
        std::size_t worker_count,
        std::size_t task_queue_capacity,
        netpulse::storage::DatabaseConfig
            database_config);

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    EventLoop(EventLoop&&) = delete;
    EventLoop& operator=(EventLoop&&) = delete;

    /// @brief 判断事件循环是否初始化成功。
    /// @return 初始化成功时返回 true，否则返回 false。
    [[nodiscard]] bool valid() const noexcept;

    /// @brief 获取事件循环初始化或运行错误码。
    /// @return 系统错误码；没有错误时为 0。
    [[nodiscard]] int errorNumber() const noexcept;

    /// @brief 启动事件循环。
    /// @return 正常运行时持续阻塞；发生致命错误时返回非零值。
    int run();

    /// @brief 获取当前活动连接数量。
    /// @return 活动客户端连接数量。
    [[nodiscard]] std::size_t connectionCount()
        const noexcept;

private:
    /// @brief 一个客户端连接及其生命周期标识。
    struct ClientState
    {
        ClientState(
            ConnectionId connection_id,
            netpulse::network::Socket socket) noexcept
            : id{connection_id}
            , connection{std::move(socket)}
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