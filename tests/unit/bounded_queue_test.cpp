#include "netpulse/concurrency/bounded_queue.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

using netpulse::concurrency::BoundedQueue;
using netpulse::concurrency::QueuePushStatus;

using namespace std::chrono_literals;

namespace {

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}

void testRejectsZeroCapacity()
{
    bool exception_thrown{false};

    try
    {
        BoundedQueue<int> queue{0};
    }
    catch (const std::invalid_argument&)
    {
        exception_thrown = true;
    }

    require(
        exception_thrown,
        "zero capacity must throw std::invalid_argument");
}

void testCapacityAndFifoOrder()
{
    BoundedQueue<int> queue{2};

    require(queue.capacity() == 2, "capacity must be two");
    require(queue.size() == 0, "new queue must be empty");
    require(!queue.closed(), "new queue must be open");

    require(
        queue.tryPush(10) == QueuePushStatus::Success,
        "first push must succeed");

    require(
        queue.tryPush(20) == QueuePushStatus::Success,
        "second push must succeed");

    require(
        queue.tryPush(30) == QueuePushStatus::Full,
        "push into full queue must fail");

    require(queue.size() == 2, "queue size must equal capacity");

    const auto first{queue.waitPop()};
    const auto second{queue.waitPop()};

    require(first.has_value() && *first == 10, "first value must be 10");
    require(second.has_value() && *second == 20, "second value must be 20");
    require(queue.size() == 0, "queue must be empty after two pops");

    require(
        queue.tryPush(30) == QueuePushStatus::Success,
        "push must succeed after space becomes available");
}

void testWaitPopWakesWhenValueArrives()
{
    BoundedQueue<int> queue{1};

    std::promise<void> consumer_started;
    std::promise<std::optional<int>> result_promise;

    auto started_future{consumer_started.get_future()};
    auto result_future{result_promise.get_future()};

    std::jthread consumer{
        [&queue, &consumer_started, &result_promise]
        {
            consumer_started.set_value();
            result_promise.set_value(queue.waitPop());
        }};

    started_future.wait();

    const auto state_before_push{
        result_future.wait_for(50ms)};

    const auto push_status{queue.tryPush(42)};
    const auto result{result_future.get()};

    consumer.join();

    require(
        state_before_push == std::future_status::timeout,
        "consumer must wait while queue is empty");

    require(
        push_status == QueuePushStatus::Success,
        "push must succeed");

    require(
        result.has_value() && *result == 42,
        "waiting consumer must receive pushed value");
}

void testCloseWakesWaitingConsumer()
{
    BoundedQueue<int> queue{1};

    std::promise<void> consumer_started;
    std::promise<std::optional<int>> result_promise;

    auto started_future{consumer_started.get_future()};
    auto result_future{result_promise.get_future()};

    std::jthread consumer{
        [&queue, &consumer_started, &result_promise]
        {
            consumer_started.set_value();
            result_promise.set_value(queue.waitPop());
        }};

    started_future.wait();

    const auto state_before_close{
        result_future.wait_for(50ms)};

    queue.close();

    const auto result{result_future.get()};
    consumer.join();

    require(
        state_before_close == std::future_status::timeout,
        "consumer must wait before queue is closed");

    require(
        !result.has_value(),
        "closed and empty queue must return std::nullopt");

    require(queue.closed(), "queue must report closed state");
}

void testCloseDrainsQueuedValues()
{
    BoundedQueue<int> queue{2};

    require(
        queue.tryPush(7) == QueuePushStatus::Success,
        "push before close must succeed");

    queue.close();

    require(
        queue.tryPush(8) == QueuePushStatus::Closed,
        "push after close must return Closed");

    const auto queued_value{queue.waitPop()};
    const auto finished{queue.waitPop()};

    require(
        queued_value.has_value() && *queued_value == 7,
        "existing value must remain available after close");

    require(
        !finished.has_value(),
        "closed and drained queue must return std::nullopt");
}

void testSupportsMoveOnlyValues()
{
    BoundedQueue<std::unique_ptr<int>> queue{1};

    auto input{std::make_unique<int>(99)};

    const auto push_status{
        queue.tryPush(std::move(input))};

    const auto output{queue.waitPop()};

    require(
        push_status == QueuePushStatus::Success,
        "move-only value push must succeed");

    require(!input, "ownership must move out of input");

    require(
        output.has_value() &&
            *output != nullptr &&
            **output == 99,
        "queue must preserve the move-only value");
}

} // namespace

int main()
{
    try
    {
        testRejectsZeroCapacity();
        testCapacityAndFifoOrder();
        testWaitPopWakesWhenValueArrives();
        testCloseWakesWaitingConsumer();
        testCloseDrainsQueuedValues();
        testSupportsMoveOnlyValues();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "bounded_queue_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout << "bounded_queue_test passed\n";
    return 0;
}