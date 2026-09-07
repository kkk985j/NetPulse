#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace netpulse::protocol {

enum class FrameQueueStatus {
    queued,
    payload_too_large
};

enum class FrameFlushStatus {
    drained,
    would_block,
    error
};

struct FrameFlushResult {
    FrameFlushStatus status{
        FrameFlushStatus::drained
    };

    std::size_t bytes_transferred{0};
    int error_number{0};

    [[nodiscard]] bool drained() const noexcept
    {
        return status == FrameFlushStatus::drained;
    }
};

class FrameWriteBuffer {
public:
    [[nodiscard]] FrameQueueStatus enqueue(
        std::span<const std::byte> payload);

    [[nodiscard]] FrameFlushResult flush(
        int fd) noexcept;

    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] std::size_t pendingBytes()
        const noexcept;

    void clear() noexcept;

private:
    void compact();

    std::vector<std::byte> buffer_{};
    std::size_t write_offset_{0};
};

}  // namespace netpulse::protocol