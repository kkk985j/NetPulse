#include "netpulse/concurrency/thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>

using netpulse::concurrency::SubmitStatus;
using netpulse::concurrency::ThreadPool;

using namespace std::chrono_literals;

namespace {

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}

void testRejectsInvalidConfiguration()
{
    bool zero_worker_rejected{false};
    bool zero_capacity_rejected{false};

    try
    {
        ThreadPool pool{0, 1};
    }
    catch (const std::invalid_argument&)
    {
        zero_worker_rejected = true;
    }

    try
    {
        ThreadPool pool{1, 0};
    }
    catch (const std::invalid_argument&)
    {
        zero_capacity_rejected = true;
    }

    require(
        zero_worker_rejected,
        "zero worker count must be rejected");

    require(
        zero_capacity_rejected,
        "zero queue capacity must be rejected");
}

void testExecutesSubmittedTasks()
{
    constexpr int task_count{8};

    ThreadPool pool{2, task_count};

    std::atomic<int> completed{0};
    std::promise<void> all_completed;

    auto completed_future{all_completed.get_future()};

    for (int index{0}; index < task_count; ++index)
    {
        const auto status{
            pool.trySubmit(
                [&completed, &all_completed]
                {
                    const int new_value{
                        completed.fetch_add(1) + 1
                    };

                    if (new_value == task_count)
                    {
                        all_completed.set_value();
                    }
                })
        };

        require(
            status == SubmitStatus::Success,
            "task submission must succeed");
    }

    const auto wait_status{
        completed_future.wait_for(1s)
    };

    pool.stop();

    require(
        wait_status == std::future_status::ready,
        "all submitted tasks must complete");

    require(
        completed.load() == task_count,
        "completed task count must match submitted count");
}

void testReportsQueueBackpressure()
{
    ThreadPool pool{1, 1};

    std::promise<void> worker_started;
    std::promise<void> release_worker;

    auto worker_started_future{
        worker_started.get_future()
    };

    auto release_future{
        release_worker.get_future().share()
    };

    const auto first_status{
        pool.trySubmit(
            [&worker_started, release_future]
            {
                worker_started.set_value();
                release_future.wait();
            })
    };

    if (first_status != SubmitStatus::Success)
    {
        release_worker.set_value();
        pool.stop();

        require(false, "blocking task submission must succeed");
    }

    if (worker_started_future.wait_for(1s) !=
        std::future_status::ready)
    {
        release_worker.set_value();
        pool.stop();

        require(false, "worker must start blocking task");
    }

    std::atomic<bool> queued_task_executed{false};

    const auto second_status{
        pool.trySubmit(
            [&queued_task_executed]
            {
                queued_task_executed.store(true);
            })
    };

    const auto third_status{
        pool.trySubmit([] {})
    };

    release_worker.set_value();
    pool.stop();

    require(
        second_status == SubmitStatus::Success,
        "one task must fit in the queue");

    require(
        third_status == SubmitStatus::QueueFull,
        "submission to full queue must report QueueFull");

    require(
        queued_task_executed.load(),
        "queued task must execute during shutdown");
}

void testTaskExceptionDoesNotStopWorker()
{
    ThreadPool pool{1, 2};

    std::promise<void> following_task_completed;
    auto completed_future{
        following_task_completed.get_future()
    };

    const auto throwing_status{
        pool.trySubmit(
            []
            {
                throw std::runtime_error{
                    "intentional task failure"
                };
            })
    };

    const auto following_status{
        pool.trySubmit(
            [&following_task_completed]
            {
                following_task_completed.set_value();
            })
    };

    const auto wait_status{
        completed_future.wait_for(1s)
    };

    pool.stop();

    require(
        throwing_status == SubmitStatus::Success,
        "throwing task must be accepted");

    require(
        following_status == SubmitStatus::Success,
        "following task must be accepted");

    require(
        wait_status == std::future_status::ready,
        "worker must continue after a task throws");
}

void testStopDrainsQueueAndRejectsNewTasks()
{
    ThreadPool pool{1, 2};

    std::promise<void> worker_started;
    std::promise<void> release_worker;

    auto started_future{
        worker_started.get_future()
    };

    auto release_future{
        release_worker.get_future().share()
    };

    const auto blocking_status{
        pool.trySubmit(
            [&worker_started, release_future]
            {
                worker_started.set_value();
                release_future.wait();
            })
    };

    if (blocking_status != SubmitStatus::Success)
    {
        release_worker.set_value();
        pool.stop();

        require(false, "blocking task must be accepted");
    }

    if (started_future.wait_for(1s) !=
        std::future_status::ready)
    {
        release_worker.set_value();
        pool.stop();

        require(false, "worker must start blocking task");
    }

    std::atomic<int> completed{0};

    const auto first_queued_status{
        pool.trySubmit(
            [&completed]
            {
                completed.fetch_add(1);
            })
    };

    const auto second_queued_status{
        pool.trySubmit(
            [&completed]
            {
                completed.fetch_add(1);
            })
    };

    release_worker.set_value();
    pool.stop();

    const auto after_stop_status{
        pool.trySubmit(ThreadPool::Task{})
    };

    const auto valid_after_stop_status{
        pool.trySubmit([] {})
    };

    pool.stop();

    require(
        first_queued_status == SubmitStatus::Success,
        "first queued task must be accepted");

    require(
        second_queued_status == SubmitStatus::Success,
        "second queued task must be accepted");

    require(
        completed.load() == 2,
        "stop must drain accepted tasks");

    require(
        after_stop_status == SubmitStatus::InvalidTask,
        "empty task must report InvalidTask");

    require(
        valid_after_stop_status == SubmitStatus::Stopped,
        "valid task after stop must report Stopped");

    require(
        pool.stopped(),
        "pool must report stopped state");
}

} // namespace

int main()
{
    try
    {
        testRejectsInvalidConfiguration();
        testExecutesSubmittedTasks();
        testReportsQueueBackpressure();
        testTaskExceptionDoesNotStopWorker();
        testStopDrainsQueueAndRejectsNewTasks();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "thread_pool_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout << "thread_pool_test passed\n";
    return 0;
}