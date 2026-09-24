#pragma once

#include "netpulse/concurrency/bounded_queue.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace netpulse::concurrency {

enum class SubmitStatus
{
    Success,
    QueueFull,
    Stopped,
    InvalidTask
};

class ThreadPool
{
public:
    using Task = std::function<void()>;

    ThreadPool(
        std::size_t worker_count,
        std::size_t queue_capacity);

    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    [[nodiscard]] SubmitStatus trySubmit(Task task);

    void stop();

    [[nodiscard]] std::size_t workerCount() const noexcept;
    [[nodiscard]] std::size_t pendingTaskCount() const;
    [[nodiscard]] bool stopped() const noexcept;

private:
    void workerLoop() noexcept;

    BoundedQueue<Task> tasks_;
    std::vector<std::thread> workers_;

    std::atomic<bool> stopped_{false};
    std::mutex stop_mutex_;
};

} // namespace netpulse::concurrency