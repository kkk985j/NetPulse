#pragma once

#include "netpulse/concurrency/thread_pool.hpp"
#include "netpulse/network/event_fd.hpp"
#include "netpulse/server/processing.hpp"
#include "netpulse/storage/database_config.hpp"

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

namespace netpulse::server {

/// @brief 将业务任务提交给线程池并通知 Reactor 处理结果。
class ProcessingDispatcher
{
public:
    /// @brief 创建不执行数据库持久化的任务调度器。
    ///
    /// 该构造函数主要用于不依赖外部数据库的单元测试。
    ///
    /// @param worker_count 工作线程数量。
    /// @param task_queue_capacity 最大等待任务数量。
    ProcessingDispatcher(
        std::size_t worker_count,
        std::size_t task_queue_capacity);

    /// @brief 创建支持数据库持久化的任务调度器。
    /// @param worker_count 工作线程数量。
    /// @param task_queue_capacity 最大等待任务数量。
    /// @param database_config 数据库连接配置。
    ProcessingDispatcher(
        std::size_t worker_count,
        std::size_t task_queue_capacity,
        netpulse::storage::DatabaseConfig
            database_config);

    ProcessingDispatcher(
        const ProcessingDispatcher&) = delete;

    ProcessingDispatcher& operator=(
        const ProcessingDispatcher&) = delete;

    ProcessingDispatcher(
        ProcessingDispatcher&&) = delete;

    ProcessingDispatcher& operator=(
        ProcessingDispatcher&&) = delete;

    /// @brief 判断调度器是否可以接收任务。
    /// @return 调度器有效时返回 true，否则返回 false。
    [[nodiscard]] bool valid() const noexcept;

    /// @brief 获取调度器最近一次后台错误码。
    /// @return 错误码；没有错误时为 0。
    [[nodiscard]] int errorNumber() const noexcept;

    /// @brief 获取用于通知 Reactor 的 eventfd。
    /// @return eventfd 文件描述符。
    [[nodiscard]] int notificationFd() const noexcept;

    /// @brief 尝试提交一项后台处理任务。
    /// @param task 等待处理的客户端数据帧。
    /// @return 任务提交状态。
    [[nodiscard]]
    netpulse::concurrency::SubmitStatus trySubmit(
        ProcessingTask task);

    /// @brief 消费 eventfd 中的完成通知。
    /// @return eventfd 读取结果。
    [[nodiscard]]
    netpulse::network::EventFdReadResult
        consumeNotifications() const noexcept;

    /// @brief 取出当前已经完成的全部处理结果。
    /// @return 已完成的处理结果列表。
    [[nodiscard]]
    std::vector<ProcessingResult> takeCompleted();

    /// @brief 停止接收任务并等待工作线程退出。
    void stop();

private:
    /// @brief 发布后台任务处理结果并唤醒 Reactor。
    /// @param result 后台处理结果。
    void publish(ProcessingResult result);

    netpulse::network::EventFd notifier_{};

    mutable std::mutex completed_mutex_;
    std::deque<ProcessingResult> completed_{};

    std::atomic<int> error_number_{0};

    // 配置构造完成后不再修改，可以被多个 Worker 并发读取。
    std::optional<netpulse::storage::DatabaseConfig>
        database_config_{};

    // 必须放在最后，使其析构时最先停止并等待工作线程。
    netpulse::concurrency::ThreadPool pool_;
};

} // namespace netpulse::server