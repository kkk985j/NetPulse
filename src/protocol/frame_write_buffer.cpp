#include "netpulse/protocol/frame_write_buffer.hpp"

#include "netpulse/protocol/frame.hpp"

#include <arpa/inet.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>

namespace netpulse::protocol {

FrameQueueStatus FrameWriteBuffer::enqueue(
    std::span<const std::byte> payload)
{
    if (payload.size() > max_payload_size) {
        return FrameQueueStatus::payload_too_large;
    }

    compact();

    const auto payload_size{
        static_cast<std::uint32_t>(payload.size())
    };

    const std::uint32_t network_size{
        htonl(payload_size)
    };

    const std::size_t old_size{
        buffer_.size()
    };

    const std::size_t frame_size{
        sizeof(network_size) + payload.size()
    };

    buffer_.resize(old_size + frame_size);

    std::memcpy(
        buffer_.data() + old_size,
        &network_size,
        sizeof(network_size));

    if (!payload.empty()) {
        std::memcpy(
            buffer_.data()
                + old_size
                + sizeof(network_size),
            payload.data(),
            payload.size());
    }

    return FrameQueueStatus::queued;
}

FrameFlushResult FrameWriteBuffer::flush(
    int fd) noexcept
{
    std::size_t total_sent{0};

    while (write_offset_ < buffer_.size()) {
        const auto sent = ::send(
            fd,
            buffer_.data() + write_offset_,
            buffer_.size() - write_offset_,
            MSG_NOSIGNAL);

        if (sent > 0) {
            const auto sent_size{
                static_cast<std::size_t>(sent)
            };

            write_offset_ += sent_size;
            total_sent += sent_size;
            continue;
        }

        if (sent == 0) {
            return {
                FrameFlushStatus::error,
                total_sent,
                EPIPE
            };
        }

        const int error_number{errno};

        if (error_number == EINTR) {
            continue;
        }

        if (error_number == EAGAIN
            || error_number == EWOULDBLOCK) {
            return {
                FrameFlushStatus::would_block,
                total_sent,
                error_number
            };
        }

        return {
            FrameFlushStatus::error,
            total_sent,
            error_number
        };
    }

    clear();

    return {
        FrameFlushStatus::drained,
        total_sent,
        0
    };
}

bool FrameWriteBuffer::empty() const noexcept
{
    return pendingBytes() == 0;
}

std::size_t FrameWriteBuffer::pendingBytes()
    const noexcept
{
    return buffer_.size() - write_offset_;
}

void FrameWriteBuffer::clear() noexcept
{
    buffer_.clear();
    write_offset_ = 0;
}

void FrameWriteBuffer::compact()
{
    if (write_offset_ == 0) {
        return;
    }

    if (write_offset_ == buffer_.size()) {
        clear();
        return;
    }

    buffer_.erase(
        buffer_.begin(),
        buffer_.begin()
            + static_cast<std::ptrdiff_t>(
                write_offset_));

    write_offset_ = 0;
}

}  // namespace netpulse::protocol