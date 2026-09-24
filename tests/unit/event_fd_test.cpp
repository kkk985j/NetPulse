#include "netpulse/network/event_fd.hpp"

#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <utility>

using netpulse::network::EventFd;

namespace {

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}

void testCreatesValidDescriptor()
{
    EventFd event_fd{};

    require(
        event_fd.valid(),
        "EventFd must create a valid descriptor");

    require(
        event_fd.fd() >= 0,
        "valid EventFd must expose a descriptor");
}

void testEmptyReadIsNonBlocking()
{
    EventFd event_fd{};

    const auto result{event_fd.consume()};

    require(
        !result,
        "empty EventFd must not produce a value");

    require(
        result.error_number == EAGAIN ||
            result.error_number == EWOULDBLOCK,
        "empty EventFd must report EAGAIN");
}

void testAggregatesNotifications()
{
    EventFd event_fd{};

    const auto first_notify{event_fd.notify()};
    const auto second_notify{event_fd.notify()};

    const auto read_result{event_fd.consume()};

    require(
        first_notify.success,
        "first notify must succeed");

    require(
        second_notify.success,
        "second notify must succeed");

    require(
        read_result.success,
        "consume after notify must succeed");

    require(
        read_result.value == 2,
        "EventFd must aggregate notification values");
}

void testRejectsZeroNotification()
{
    EventFd event_fd{};

    const auto result{event_fd.notify(0)};

    require(
        !result,
        "zero notification must be rejected");

    require(
        result.error_number == EINVAL,
        "zero notification must report EINVAL");
}

void testSupportsMoveOwnership()
{
    EventFd original{};
    const int descriptor{original.fd()};

    EventFd moved{std::move(original)};

    require(
        !original.valid(),
        "moved-from EventFd must be invalid");

    require(
        moved.valid(),
        "moved-to EventFd must remain valid");

    require(
        moved.fd() == descriptor,
        "move must preserve descriptor ownership");

    const auto notify_result{moved.notify()};

    require(
        notify_result.success,
        "moved EventFd must remain usable");

    const auto read_result{moved.consume()};

    require(
        read_result && read_result.value == 1,
        "moved EventFd must preserve behavior");
}

} // namespace

int main()
{
    try
    {
        testCreatesValidDescriptor();
        testEmptyReadIsNonBlocking();
        testAggregatesNotifications();
        testRejectsZeroNotification();
        testSupportsMoveOwnership();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "event_fd_test failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    std::cout << "event_fd_test passed\n";
    return 0;
}