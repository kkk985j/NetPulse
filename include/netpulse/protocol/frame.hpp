#pragma once

#include "netpulse/network/io.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace netpulse::protocol {
    inline constexpr std::uint32_t max_payload_size
    {
        1024U * 1024U
    };

    enum class FrameStatus
    {
        completed,
        peer_closed,
        would_block,
        payload_too_large,
        io_error
    };

    struct SendFrameResult
    {
        FrameStatus status {FrameStatus::completed};
        std::size_t bytes_transferred {0};
        int error_number {0};

        [[nodiscard]] bool completed() const noexcept
        {
            return status == FrameStatus::completed;
        }
    };

    struct ReceiveFrameResult
    {
        FrameStatus status {FrameStatus::completed};
        std::vector<std::byte> payload {};
        std::size_t bytes_transferred {0};
        int error_number {0};

        [[nodiscard]] bool completed() const noexcept
        {
            return status == FrameStatus::completed;
        }
    };

    [[nodiscard]] SendFrameResult sendFrame
    (
        int fd,
        std::span<const std::byte> payload
    ) noexcept;

    [[nodiscard]] ReceiveFrameResult receiveFrame
    (
        int fd
    ) noexcept;

} // namespcace netpulse::protocol