#pragma once

#include "netpulse/concurrency/thread_pool.hpp"
#include "netpulse/network/event_fd.hpp"
#include "netpulse/server/processing.hpp"

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

namespace netpulse::server {

class ProcessingDispatcher
{
public:
    ProcessingDispatcher(
        std::size_t worker_count,
        std::size_t task_queue_capacity);

    ProcessingDispatcher(
        const ProcessingDispatcher&) = delete;

    ProcessingDispatcher& operator=(
        const ProcessingDispatcher&) = delete;

    ProcessingDispatcher(
        ProcessingDispatcher&&) = delete;

    ProcessingDispatcher& operator=(
        ProcessingDispatcher&&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] int errorNumber() const noexcept;
    [[nodiscard]] int notificationFd() const noexcept;

    [[nodiscard]]
    netpulse::concurrency::SubmitStatus trySubmit(
        ProcessingTask task);

    [[nodiscard]]
    netpulse::network::EventFdReadResult
        consumeNotifications() const noexcept;

    [[nodiscard]]
    std::vector<ProcessingResult> takeCompleted();

    void stop();

private:
    void publish(ProcessingResult result);

    netpulse::network::EventFd notifier_{};

    mutable std::mutex completed_mutex_;
    std::deque<ProcessingResult> completed_{};

    std::atomic<int> error_number_{0};

    // 必须放在最后，使其析构时最先停止并等待工作线程。
    netpulse::concurrency::ThreadPool pool_;
};

} // namespace netpulse::server