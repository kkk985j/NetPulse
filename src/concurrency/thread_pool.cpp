#include "netpulse/concurrency/thread_pool.hpp"

#include <stdexcept>
#include <utility>

namespace netpulse::concurrency {

ThreadPool::ThreadPool(
    std::size_t worker_count,
    std::size_t queue_capacity)
    : tasks_{queue_capacity}
{
    if (worker_count == 0)
    {
        throw std::invalid_argument{
            "ThreadPool worker count must be greater than zero"
        };
    }

    workers_.reserve(worker_count);

    try
    {
        for (std::size_t index{0};
             index < worker_count;
             ++index)
        {
            workers_.emplace_back(
                &ThreadPool::workerLoop,
                this);
        }
    }
    catch (...)
    {
        tasks_.close();

        for (auto& worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        throw;
    }
}

ThreadPool::~ThreadPool()
{
    stop();
}

SubmitStatus ThreadPool::trySubmit(Task task)
{
    if (!task)
    {
        return SubmitStatus::InvalidTask;
    }

    const QueuePushStatus push_status{
        tasks_.tryPush(std::move(task))
    };

    switch (push_status)
    {
    case QueuePushStatus::Success:
        return SubmitStatus::Success;

    case QueuePushStatus::Full:
        return SubmitStatus::QueueFull;

    case QueuePushStatus::Closed:
        return SubmitStatus::Stopped;
    }

    return SubmitStatus::Stopped;
}

void ThreadPool::stop()
{
    std::lock_guard lock{stop_mutex_};

    if (!stopped_.exchange(true))
    {
        tasks_.close();
    }

    for (auto& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

std::size_t ThreadPool::workerCount() const noexcept
{
    return workers_.size();
}

std::size_t ThreadPool::pendingTaskCount() const
{
    return tasks_.size();
}

bool ThreadPool::stopped() const noexcept
{
    return stopped_.load();
}

void ThreadPool::workerLoop() noexcept
{
    while (true)
    {
        auto task{tasks_.waitPop()};

        if (!task.has_value())
        {
            return;
        }

        try
        {
            (*task)();
        }
        catch (...)
        {
            // 单个任务抛出异常时，保持工作线程继续运行。
            // 后续可以接入日志或异常回调。
        }
    }
}

} // namespace netpulse::concurrency