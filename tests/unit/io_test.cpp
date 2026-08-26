#include "netpulse/network/io.hpp"
#include "netpulse/network/socket.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <span>
#include <string_view>
#include <sys/socket.h>

using netpulse::network::IoStatus;
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

    constexpr std::string_view message{
        "reliable io works"
    };

    const auto source = std::as_bytes(
        std::span{message.data(), message.size()});

    const auto send_result = sendAll(
        sender.fd(),
        source);

    if (!send_result.completed()) {
        return fail("sendAll did not complete");
    }

    if (send_result.bytes_transferred != source.size()) {
        return fail("sendAll reported incorrect byte count");
    }

    std::array<std::byte, message.size()> destination{};

    const auto receive_result = receiveExact(
        receiver.fd(),
        std::span<std::byte>{destination});

    if (!receive_result.completed()) {
        return fail("receiveExact did not complete");
    }

    if (receive_result.bytes_transferred
        != destination.size()) {
        return fail("receiveExact reported incorrect byte count");
    }

    if (!std::equal(
            source.begin(),
            source.end(),
            destination.begin())) {
        return fail("received data does not match source");
    }

    int eof_pair[2]{-1, -1};

    if (::socketpair(
            AF_UNIX,
            SOCK_STREAM,
            0,
            eof_pair) < 0) {
        return fail("could not create EOF socket pair");
    }

    Socket eof_sender{eof_pair[0]};
    Socket eof_receiver{eof_pair[1]};

    constexpr std::array<std::byte, 3> partial_data{
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03}
    };

    const auto partial_send_result = sendAll(
        eof_sender.fd(),
        std::span<const std::byte>{partial_data});

    if (!partial_send_result.completed()) {
        return fail("could not send partial test data");
    }

    eof_sender.reset();

    std::array<std::byte, 5> incomplete_buffer{};

    const auto eof_result = receiveExact(
        eof_receiver.fd(),
        std::span<std::byte>{incomplete_buffer});

    if (eof_result.status != IoStatus::peer_closed) {
        return fail("receiveExact did not report peer closure");
    }

    if (eof_result.bytes_transferred
        != partial_data.size()) {
        return fail("peer closure byte count is incorrect");
    }

    if (!std::equal(
            partial_data.begin(),
            partial_data.end(),
            incomplete_buffer.begin())) {
        return fail("partial data was not preserved");
    }

    std::cout << "Reliable I/O tests passed.\n";
    return 0;
}