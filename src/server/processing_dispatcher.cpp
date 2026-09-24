#include "netpulse/server/processing_dispatcher.hpp"

#include <cerrno>
#include <utility>

namespace netpulse::server {

ProcessingDispatcher::ProcessingDispatcher(
    std::size_t worker_count,
    std::size_t task_queue_capacity)
    : pool_{
          worker_count,
          task_queue_capacity}
{
    if (!notifier_.valid())
    {
        error_number_.store(
            notifier_.errorNumber());

        pool_.stop();
    }
}

bool ProcessingDispatcher::valid() const noexcept
{
    return notifier_.valid() &&
        error_number_.load() == 0;
}

int ProcessingDispatcher::errorNumber() const noexcept
{
    const int background_error{
        error_number_.load()
    };

    if (background_error != 0)
    {
        return background_error;
    }

    return notifier_.errorNumber();
}

int ProcessingDispatcher::notificationFd() const noexcept
{
    return notifier_.fd();
}

netpulse::concurrency::SubmitStatus
ProcessingDispatcher::trySubmit(
    ProcessingTask task)
{
    using netpulse::concurrency::SubmitStatus;

    if (task.client_fd < 0 ||
        task.connection_id == 0)
    {
        return SubmitStatus::InvalidTask;
    }

    if (!valid())
    {
        return SubmitStatus::Stopped;
    }

    return pool_.trySubmit(
        [this, task = std::move(task)]() mutable
        {
            publish(
                processFrame(std::move(task)));
        });
}

netpulse::network::EventFdReadResult
ProcessingDispatcher::consumeNotifications()
    const noexcept
{
    return notifier_.consume();
}

std::vector<ProcessingResult>
ProcessingDispatcher::takeCompleted()
{
    std::deque<ProcessingResult> local_results;

    {
        std::lock_guard lock{completed_mutex_};
        local_results.swap(completed_);
    }

    std::vector<ProcessingResult> results;
    results.reserve(local_results.size());

    while (!local_results.empty())
    {
        results.push_back(
            std::move(local_results.front()));

        local_results.pop_front();
    }

    return results;
}

void ProcessingDispatcher::stop()
{
    pool_.stop();
}

void ProcessingDispatcher::publish(
    ProcessingResult result)
{
    {
        std::lock_guard lock{completed_mutex_};
        completed_.push_back(std::move(result));
    }

    const auto notify_result{
        notifier_.notify()
    };

    if (!notify_result.success &&
        notify_result.error_number != EAGAIN &&
        notify_result.error_number != EWOULDBLOCK)
    {
        int expected_error{0};

        error_number_.compare_exchange_strong(
            expected_error,
            notify_result.error_number);
    }
}

} // namespace netpulse::server