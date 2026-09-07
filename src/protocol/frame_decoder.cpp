#include "netpulse/protocol/frame_decoder.hpp"

#include "netpulse/protocol/frame.hpp"

#include <arpa/inet.h>

#include <cstdint>
#include <cstring>
#include <utility>

namespace netpulse::protocol {

namespace {

constexpr std::size_t kFrameHeaderSize{
    sizeof(std::uint32_t)
};

}  // namespace

void FrameDecoder::append(
    std::span<const std::byte> data)
{
    buffer_.insert(
        buffer_.end(),
        data.begin(),
        data.end());
}

FrameDecodeResult FrameDecoder::decodeNext()
{
    if (bufferedBytes() < kFrameHeaderSize) {
        return {
            FrameDecodeStatus::need_more_data,
            {}
        };
    }

    std::uint32_t network_payload_size{0};

    std::memcpy(
        &network_payload_size,
        buffer_.data() + read_offset_,
        kFrameHeaderSize);

    const std::uint32_t payload_size{
        ntohl(network_payload_size)
    };

    if (payload_size > max_payload_size) {
        return {
            FrameDecodeStatus::payload_too_large,
            {}
        };
    }

    const std::size_t complete_frame_size{
        kFrameHeaderSize
        + static_cast<std::size_t>(payload_size)
    };

    if (bufferedBytes() < complete_frame_size) {
        return {
            FrameDecodeStatus::need_more_data,
            {}
        };
    }

    std::vector<std::byte> payload(payload_size);

    if (payload_size > 0) {
        std::memcpy(
            payload.data(),
            buffer_.data()
                + read_offset_
                + kFrameHeaderSize,
            payload_size);
    }

    read_offset_ += complete_frame_size;
    compact();

    return {
        FrameDecodeStatus::frame_ready,
        std::move(payload)
    };
}

std::size_t FrameDecoder::bufferedBytes()
    const noexcept
{
    return buffer_.size() - read_offset_;
}

void FrameDecoder::clear() noexcept
{
    buffer_.clear();
    read_offset_ = 0;
}

void FrameDecoder::compact()
{
    if (read_offset_ == 0) {
        return;
    }

    if (read_offset_ == buffer_.size()) {
        buffer_.clear();
        read_offset_ = 0;
        return;
    }

    constexpr std::size_t kCompactionThreshold{
        4096
    };

    if (read_offset_ >= kCompactionThreshold
        && read_offset_ >= buffer_.size() / 2) {
        buffer_.erase(
            buffer_.begin(),
            buffer_.begin()
                + static_cast<std::ptrdiff_t>(
                    read_offset_));

        read_offset_ = 0;
    }
}

}  // namespace netpulse::protocol