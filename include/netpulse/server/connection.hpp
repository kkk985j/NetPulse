#pragma once

#include "netpulse/network/socket.hpp"
#include "netpulse/protocol/frame_decoder.hpp"
#include "netpulse/protocol/frame_write_buffer.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace netpulse::server {

enum class ConnectionReadStatus {
    ready,
    peer_closed,
    protocol_error,
    io_error
};

struct ConnectionReadResult {
    ConnectionReadStatus status{
        ConnectionReadStatus::ready
    };

    std::vector<std::vector<std::byte>> frames{};
    int error_number{0};
};

class Connection {
public:
    explicit Connection(
        netpulse::network::Socket socket) noexcept;

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&&) noexcept = default;
    Connection& operator=(Connection&&) noexcept = default;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] int fd() const noexcept;

    [[nodiscard]] ConnectionReadResult
        readAvailable();

    [[nodiscard]]
    netpulse::protocol::FrameQueueStatus queueFrame(
        std::span<const std::byte> payload);

    [[nodiscard]]
    netpulse::protocol::FrameFlushResult
        flushWrites() noexcept;

    [[nodiscard]] bool wantsWrite() const noexcept;

private:
    netpulse::network::Socket socket_{};
    netpulse::protocol::FrameDecoder decoder_{};
    netpulse::protocol::FrameWriteBuffer
        write_buffer_{};
};

}  // namespace netpulse::server