#include "netpulse/network/socket.hpp"
#include "netpulse/protocol/frame.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <sys/socket.h>
#include <vector>

using netpulse::network::Socket;
using netpulse::protocol::FrameStatus;
using netpulse::protocol::max_payload_size;
using netpulse::protocol::receiveFrame;
using netpulse::protocol::sendFrame;

namespace {

int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

bool payloadMatches(
    std::span<const std::byte> expected,
    const std::vector<std::byte>& actual)
{
    return expected.size() == actual.size()
        && std::equal(
            expected.begin(),
            expected.end(),
            actual.begin());
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

    constexpr std::string_view first_message{
        "temperature=25.4"
    };

    constexpr std::string_view second_message{
        "humidity=61"
    };

    const auto first_payload = std::as_bytes(
        std::span{
            first_message.data(),
            first_message.size()
        });

    const auto second_payload = std::as_bytes(
        std::span{
            second_message.data(),
            second_message.size()
        });

    const auto first_send = sendFrame(
        sender.fd(),
        first_payload);

    if (!first_send.completed()) {
        return fail("could not send first frame");
    }

    if (first_send.bytes_transferred
        != sizeof(std::uint32_t) + first_payload.size()) {
        return fail("first frame byte count is incorrect");
    }

    const auto second_send = sendFrame(
        sender.fd(),
        second_payload);

    if (!second_send.completed()) {
        return fail("could not send second frame");
    }

    if (second_send.bytes_transferred
        != sizeof(std::uint32_t) + second_payload.size()) {
        return fail("second frame byte count is incorrect");
    }

    const auto first_receive = receiveFrame(
        receiver.fd());

    if (!first_receive.completed()) {
        return fail("could not receive first frame");
    }

    if (!payloadMatches(
            first_payload,
            first_receive.payload)) {
        return fail("first payload does not match");
    }

    const auto second_receive = receiveFrame(
        receiver.fd());

    if (!second_receive.completed()) {
        return fail("could not receive second frame");
    }

    if (!payloadMatches(
            second_payload,
            second_receive.payload)) {
        return fail("second payload does not match");
    }

    std::vector<std::byte> oversized_payload(
        static_cast<std::size_t>(max_payload_size) + 1U);

    const auto oversized_result = sendFrame(
        sender.fd(),
        std::span<const std::byte>{oversized_payload});

    if (oversized_result.status
        != FrameStatus::payload_too_large) {
        return fail("oversized payload was not rejected");
    }

    if (oversized_result.bytes_transferred != 0) {
        return fail("oversized payload transferred data");
    }

    std::cout << "Frame protocol tests passed.\n";
    return 0;
}