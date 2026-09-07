#include "netpulse/network/epoll.hpp"
#include "netpulse/network/io.hpp"
#include "netpulse/network/socket.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <span>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <utility>

using netpulse::network::Epoll;
using netpulse::network::Socket;
using netpulse::network::receiveExact;
using netpulse::network::sendAll;

namespace {

int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

}  // namespace

int main()
{
    int socket_pair[2]{-1, -1};

    if (::socketpair(
            AF_UNIX,
            SOCK_STREAM,
            0,
            socket_pair) < 0) {
        return fail("could not create socket pair");
    }

    Socket sender{socket_pair[0]};
    Socket receiver{socket_pair[1]};

    Epoll poller{};

    if (!poller.valid()) {
        return fail("could not create epoll instance");
    }

    const auto add_result = poller.add(
        receiver.fd(),
        EPOLLIN);

    if (!add_result) {
        return fail("could not add descriptor");
    }

    const auto modify_result = poller.modify(
        receiver.fd(),
        EPOLLIN | EPOLLRDHUP);

    if (!modify_result) {
        return fail("could not modify descriptor");
    }

    constexpr std::array<std::byte, 1> payload{
        std::byte{0x42}
    };

    const auto send_result = sendAll(
        sender.fd(),
        std::span<const std::byte>{payload});

    if (!send_result.completed()) {
        return fail("could not send test byte");
    }

    std::array<epoll_event, 4> events{};

    const auto wait_result = poller.wait(
        std::span<epoll_event>{events},
        1000);

    if (!wait_result.completed()) {
        return fail("epoll wait failed");
    }

    if (wait_result.event_count != 1) {
        return fail("unexpected event count");
    }

    if (events[0].data.fd != receiver.fd()) {
        return fail("event contains incorrect descriptor");
    }

    if ((events[0].events & EPOLLIN) == 0) {
        return fail("EPOLLIN was not reported");
    }

    std::array<std::byte, 1> received{};

    const auto receive_result = receiveExact(
        receiver.fd(),
        std::span<std::byte>{received});

    if (!receive_result.completed()) {
        return fail("could not receive test byte");
    }

    if (received != payload) {
        return fail("received byte does not match");
    }

    Epoll moved_poller{std::move(poller)};

    if (poller.valid()) {
        return fail("moved-from epoll is still valid");
    }

    if (!moved_poller.valid()) {
        return fail("moved epoll is invalid");
    }

    sender.reset();

    events = {};

    const auto close_wait_result = moved_poller.wait(
        std::span<epoll_event>{events},
        1000);

    if (!close_wait_result.completed()) {
        return fail("waiting for peer closure failed");
    }

    if (close_wait_result.event_count != 1) {
        return fail("peer closure event was not reported");
    }

    if ((events[0].events
         & (EPOLLRDHUP | EPOLLHUP)) == 0) {
        return fail("closure flags were not reported");
    }

    const auto remove_result = moved_poller.remove(
        receiver.fd());

    if (!remove_result) {
        return fail("could not remove descriptor");
    }

    events = {};

    const auto empty_wait_result = moved_poller.wait(
        std::span<epoll_event>{events},
        0);

    if (!empty_wait_result.completed()) {
        return fail("empty epoll wait failed");
    }

    if (empty_wait_result.event_count != 0) {
        return fail("removed descriptor still produced events");
    }

    std::cout << "Epoll tests passed.\n";
    return 0;
}