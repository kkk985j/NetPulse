#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace netpulse::protocol {

enum class FrameDecodeStatus {
    need_more_data,
    frame_ready,
    payload_too_large
};

struct FrameDecodeResult {
    FrameDecodeStatus status{
        FrameDecodeStatus::need_more_data
    };

    std::vector<std::byte> payload{};
};

class FrameDecoder {
public:
    void append(std::span<const std::byte> data);

    [[nodiscard]] FrameDecodeResult decodeNext();

    [[nodiscard]] std::size_t bufferedBytes()
        const noexcept;

    void clear() noexcept;

private:
    void compact();

    std::vector<std::byte> buffer_{};
    std::size_t read_offset_{0};
};

}  // namespace netpulse::protocol