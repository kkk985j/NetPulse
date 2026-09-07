#include "netpulse/network/nonblocking.hpp"
#include "netpulse/network/socket.hpp"
#include "netpulse/protocol/frame.hpp"
#include "netpulse/protocol/frame_write_buffer.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <span>
#include <string_view>
#include <sys/socket.h>
#include <vector>

using netpulse::network::Socket;
using netpulse::network::setNonBlocking;
using netpulse::protocol::FrameFlushStatus;
using netpulse::protocol::FrameQueueStatus;
using netpulse::protocol::FrameWriteBuffer;
using netpulse::protocol::max_payload_size;
using netpulse::protocol::receiveFrame;

namespace {

int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

std::span<const std::byte> asBytes(
    std::string_view text)
{
    return std::as_bytes(
        std::span{
            text.data(),
            text.size()
        });
}

bool payloadMatches(
    const std::vector<std::byte>& actual,
    std::string_view expected)
{
    const auto expected_bytes = asBytes(expected);

    return actual.size() == expected_bytes.size()
        && std::equal(
            actual.begin(),
            actual.end(),
            expected_bytes.begin());
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

    FrameWriteBuffer write_buffer{};

    constexpr std::string_view first_message{
        "first"
    };

    constexpr std::string_view second_message{
        "second"
    };

    if (write_buffer.enqueue(asBytes(first_message))
        != FrameQueueStatus::queued) {
        return fail("could not queue first frame");
    }

    if (write_buffer.enqueue(asBytes(second_message))
        != FrameQueueStatus::queued) {
        return fail("could not queue second frame");
    }

    const std::size_t expected_bytes{
        4U + first_message.size()
        + 4U + second_message.size()
    };

    if (write_buffer.pendingBytes()
        != expected_bytes) {
        return fail("queued byte count is incorrect");
    }

    const auto flush_result = write_buffer.flush(
        sender.fd());

    if (!flush_result.drained()) {
        return fail("blocking flush did not complete");
    }

    if (flush_result.bytes_transferred
        != expected_bytes) {
        return fail("flush byte count is incorrect");
    }

    if (!write_buffer.empty()) {
        return fail("drained buffer is not empty");
    }

    const auto first_receive = receiveFrame(
        receiver.fd());

    if (!first_receive.completed()
        || !payloadMatches(
            first_receive.payload,
            first_message)) {
        return fail("first received frame is incorrect");
    }

    const auto second_receive = receiveFrame(
        receiver.fd());

    if (!second_receive.completed()
        || !payloadMatches(
            second_receive.payload,
            second_message)) {
        return fail("second received frame is incorrect");
    }

    int pressure_pair[2]{-1, -1};

    if (::socketpair(
            AF_UNIX,
            SOCK_STREAM,
            0,
            pressure_pair) < 0) {
        return fail("could not create pressure socket pair");
    }

    Socket pressured_sender{pressure_pair[0]};
    Socket pressured_receiver{pressure_pair[1]};

    constexpr int small_send_buffer{4096};

    if (::setsockopt(
            pressured_sender.fd(),
            SOL_SOCKET,
            SO_SNDBUF,
            &small_send_buffer,
            sizeof(small_send_buffer)) < 0) {
        return fail("could not reduce send buffer");
    }

    const auto nonblocking_result = setNonBlocking(
        pressured_sender.fd());

    if (!nonblocking_result) {
        return fail("could not enable non-blocking mode");
    }

    FrameWriteBuffer pressured_buffer{};

    std::vector<std::byte> large_payload(
        max_payload_size,
        std::byte{0x5A});

    if (pressured_buffer.enqueue(
            std::span<const std::byte>{large_payload})
        != FrameQueueStatus::queued) {
        return fail("could not queue large frame");
    }

    const std::size_t pending_before{
        pressured_buffer.pendingBytes()
    };

    const auto partial_result = pressured_buffer.flush(
        pressured_sender.fd());

    if (partial_result.status
        != FrameFlushStatus::would_block) {
        return fail("flush did not report would_block");
    }

    if (partial_result.bytes_transferred == 0) {
        return fail("flush made no initial progress");
    }

    if (pressured_buffer.pendingBytes() == 0
        || pressured_buffer.pendingBytes()
            >= pending_before) {
        return fail("partial-send position is incorrect");
    }

    pressured_buffer.clear();

    if (!pressured_buffer.empty()) {
        return fail("clear did not empty the buffer");
    }

    std::vector<std::byte> oversized_payload(
        static_cast<std::size_t>(max_payload_size)
            + 1U);

    if (pressured_buffer.enqueue(
            std::span<const std::byte>{
                oversized_payload
            })
        != FrameQueueStatus::payload_too_large) {
        return fail("oversized payload was accepted");
    }

    if (!pressured_buffer.empty()) {
        return fail("oversized payload changed the buffer");
    }

    std::cout << "Frame write buffer tests passed.\n";
    return 0;
}