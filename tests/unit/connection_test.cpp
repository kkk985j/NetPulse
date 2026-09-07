#include "netpulse/network/io.hpp"
#include "netpulse/network/nonblocking.hpp"
#include "netpulse/network/socket.hpp"
#include "netpulse/protocol/frame.hpp"
#include "netpulse/protocol/frame_write_buffer.hpp"
#include "netpulse/server/connection.hpp"

#include <arpa/inet.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <sys/socket.h>
#include <utility>
#include <vector>

using netpulse::network::Socket;
using netpulse::network::sendAll;
using netpulse::network::setNonBlocking;
using netpulse::protocol::FrameQueueStatus;
using netpulse::protocol::max_payload_size;
using netpulse::protocol::receiveFrame;
using netpulse::protocol::sendFrame;
using netpulse::server::Connection;
using netpulse::server::ConnectionReadStatus;

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

    Socket peer{socket_pair[0]};
    Socket connection_socket{socket_pair[1]};

    if (!setNonBlocking(connection_socket.fd())) {
        return fail("could not enable non-blocking mode");
    }

    Connection connection{
        std::move(connection_socket)
    };

    if (connection_socket.valid()) {
        return fail("socket ownership was not transferred");
    }

    if (!connection.valid()) {
        return fail("connection is invalid");
    }

    constexpr std::string_view first_message{
        "temperature=25.4"
    };

    constexpr std::string_view second_message{
        "humidity=61"
    };

    if (!sendFrame(peer.fd(), asBytes(first_message))
             .completed()) {
        return fail("could not send first frame");
    }

    if (!sendFrame(peer.fd(), asBytes(second_message))
             .completed()) {
        return fail("could not send second frame");
    }

    const auto read_result{
        connection.readAvailable()
    };

    if (read_result.status
        != ConnectionReadStatus::ready) {
        return fail("connection read failed");
    }

    if (read_result.frames.size() != 2) {
        return fail("connection did not return two frames");
    }

    if (!payloadMatches(
            read_result.frames[0],
            first_message)) {
        return fail("first frame does not match");
    }

    if (!payloadMatches(
            read_result.frames[1],
            second_message)) {
        return fail("second frame does not match");
    }

    if (connection.wantsWrite()) {
        return fail("new connection unexpectedly wants write");
    }

    constexpr std::string_view response{
        "ACK from Connection"
    };

    if (connection.queueFrame(asBytes(response))
        != FrameQueueStatus::queued) {
        return fail("could not queue response");
    }

    if (!connection.wantsWrite()) {
        return fail("queued response did not enable write");
    }

    const auto flush_result{
        connection.flushWrites()
    };

    if (!flush_result.drained()) {
        return fail("response flush did not complete");
    }

    if (connection.wantsWrite()) {
        return fail("drained connection still wants write");
    }

    const auto response_result{
        receiveFrame(peer.fd())
    };

    if (!response_result.completed()
        || !payloadMatches(
            response_result.payload,
            response)) {
        return fail("peer received incorrect response");
    }

    Connection moved_connection{
        std::move(connection)
    };

    if (connection.valid()) {
        return fail("moved-from connection is still valid");
    }

    if (!moved_connection.valid()) {
        return fail("moved connection is invalid");
    }

    peer.reset();

    const auto close_result{
        moved_connection.readAvailable()
    };

    if (close_result.status
        != ConnectionReadStatus::peer_closed) {
        return fail("peer closure was not reported");
    }

    int protocol_pair[2]{-1, -1};

    if (::socketpair(
            AF_UNIX,
            SOCK_STREAM,
            0,
            protocol_pair) < 0) {
        return fail("could not create protocol socket pair");
    }

    Socket protocol_peer{protocol_pair[0]};
    Socket protocol_socket{protocol_pair[1]};

    if (!setNonBlocking(protocol_socket.fd())) {
        return fail("could not configure protocol socket");
    }

    Connection protocol_connection{
        std::move(protocol_socket)
    };

    const std::uint32_t oversized_length{
        htonl(max_payload_size + 1U)
    };

    const auto oversized_header = std::as_bytes(
        std::span<const std::uint32_t>{
            &oversized_length,
            1
        });

    if (!sendAll(
            protocol_peer.fd(),
            oversized_header).completed()) {
        return fail("could not send oversized header");
    }

    const auto protocol_result{
        protocol_connection.readAvailable()
    };

    if (protocol_result.status
        != ConnectionReadStatus::protocol_error) {
        return fail("protocol error was not reported");
    }

    std::cout << "Connection tests passed.\n";
    return 0;
}