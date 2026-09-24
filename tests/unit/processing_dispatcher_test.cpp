#include "netpulse/network/epoll.hpp"
#include "netpulse/server/processing_dispatcher.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>

using netpulse::concurrency::SubmitStatus;
using netpulse::network::Epoll;
using netpulse::server::ProcessingDispatcher;
using netpulse::server::ProcessingTask;

namespace {

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}

std::string toString(
    const std::vector<std::byte>& bytes)
{
    return {
        reinterpret_cast<const char*>(
            bytes.data()),
        bytes.size()
    };
}

void testRejectsInvalidTask()
{
    ProcessingDispatcher dispatcher{1, 4};

    const auto invalid_fd_status{
        dispatcher.trySubmit(
            ProcessingTask{-1, 1, {}})
    };

    const auto invalid_id_status{
        dispatcher.trySubmit(
            ProcessingTask{7, 0, {}})
    };

    require(
        invalid_fd_status ==
            SubmitStatus::InvalidTask,
        "negative fd must be rejected");

    require(
        invalid_id_status ==
            SubmitStatus::InvalidTask,
        "zero connection id must be rejected");

    dispatcher.stop();
}

void testPublishesCompletedResult()
{
    ProcessingDispatcher dispatcher{1, 4};
    Epoll epoll{};

    require(
        dispatcher.valid(),
        "dispatcher must be valid");

    require(
        epoll.valid(),
        "epoll must be valid");

    const auto add_result{
        epoll.add(
            dispatcher.notificationFd(),
            EPOLLIN)
    };

    require(
        static_cast<bool>(add_result),
        "notification fd must register with epoll");

    const auto submit_status{
        dispatcher.trySubmit(
            ProcessingTask{
                42,
                1001,
                {}})
    };

    require(
        submit_status == SubmitStatus::Success,
        "valid processing task must be accepted");

    std::array<epoll_event, 1> events{};

    const auto wait_result{
        epoll.wait(
            std::span<epoll_event>{events},
            1000)
    };

    require(
        wait_result.completed(),
        "epoll wait must complete");

    require(
        wait_result.event_count == 1,
        "dispatcher must wake epoll");

    require(
        events[0].data.fd ==
            dispatcher.notificationFd(),
        "ready fd must be dispatcher notifier");

    const auto notification{
        dispatcher.consumeNotifications()
    };

    require(
        notification.success,
        "notification must be consumable");

    require(
        notification.value >= 1,
        "notification count must be positive");

    auto results{dispatcher.takeCompleted()};

    require(
        results.size() == 1,
        "one task must produce one result");

    require(
        results[0].client_fd == 42,
        "result must preserve client fd");

    require(
        results[0].connection_id == 1001,
        "result must preserve connection id");

    require(
        toString(results[0].response) ==
            "ACK from NetPulse!",
        "result must contain ACK response");

    require(
        dispatcher.takeCompleted().empty(),
        "taking results must drain the queue");

    dispatcher.stop();
}

void testRejectsTasksAfterStop()
{
    ProcessingDispatcher dispatcher{1, 4};

    dispatcher.stop();

    const auto status{
        dispatcher.trySubmit(
            ProcessingTask{
                8,
                2002,
                {}})
    };

    require(
        status == SubmitStatus::Stopped,
        "stopped dispatcher must reject tasks");
}

} // namespace

int main()
{
    try
    {
        testRejectsInvalidTask();
        testPublishesCompletedResult();
        testRejectsTasksAfterStop();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "processing_dispatcher_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout
        << "processing_dispatcher_test passed\n";

    return 0;
}