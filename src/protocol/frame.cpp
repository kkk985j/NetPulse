#include "netpulse/protocol/frame.hpp"

#include <arpa/inet.h>

#include <array>
#include <cstring>

namespace netpulse::protocol {

namespace {

[[nodiscard]] FrameStatus toFrameStatus(
    netpulse::network::IoStatus status) noexcept
{
    using netpulse::network::IoStatus;

    switch (status) {
    case IoStatus::completed:
        return FrameStatus::completed;

    case IoStatus::peer_closed:
        return FrameStatus::peer_closed;

    case IoStatus::would_block:
        return FrameStatus::would_block;

    case IoStatus::error:
        return FrameStatus::io_error;
    }

    return FrameStatus::io_error;
}

}  // namespace

SendFrameResult sendFrame(
    int fd,
    std::span<const std::byte> payload) noexcept
{
    using netpulse::network::sendAll;

    if (payload.size() > max_payload_size) {
        return {
            FrameStatus::payload_too_large,
            0,
            0
        };
    }

    const auto payload_size =
        static_cast<std::uint32_t>(payload.size());

    const std::uint32_t network_size{
        htonl(payload_size)
    };

    const auto header = std::as_bytes(
        std::span<const std::uint32_t>{
            &network_size,
            1
        });

    const auto header_result = sendAll(fd, header);

    if (!header_result.completed()) {
        return {
            toFrameStatus(header_result.status),
            header_result.bytes_transferred,
            header_result.error_number
        };
    }

    const auto payload_result = sendAll(fd, payload);

    return {
        toFrameStatus(payload_result.status),
        header_result.bytes_transferred
            + payload_result.bytes_transferred,
        payload_result.error_number
    };
}

ReceiveFrameResult receiveFrame(int fd) noexcept
{
    using netpulse::network::receiveExact;

    std::array<std::byte, sizeof(std::uint32_t)> header{};

    const auto header_result = receiveExact(
        fd,
        std::span<std::byte>{header});

    if (!header_result.completed()) {
        return {
            toFrameStatus(header_result.status),
            {},
            header_result.bytes_transferred,
            header_result.error_number
        };
    }

    std::uint32_t network_size{0};

    std::memcpy(
        &network_size,
        header.data(),
        sizeof(network_size));

    const std::uint32_t payload_size{
        ntohl(network_size)
    };

    if (payload_size > max_payload_size) {
        return {
            FrameStatus::payload_too_large,
            {},
            header.size(),
            0
        };
    }

    std::vector<std::byte> payload(payload_size);

    const auto payload_result = receiveExact(
        fd,
        std::span<std::byte>{payload});

    return {
        toFrameStatus(payload_result.status),
        std::move(payload),
        header_result.bytes_transferred
            + payload_result.bytes_transferred,
        payload_result.error_number
    };
}

}  // namespace netpulse::protocol