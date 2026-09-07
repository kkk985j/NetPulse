#include "netpulse/server/connection.hpp"

#include <array>
#include <cerrno>
#include <sys/socket.h>
#include <utility>

namespace netpulse::server {

Connection::Connection(
    netpulse::network::Socket socket) noexcept
    : socket_{std::move(socket)}
{
}

bool Connection::valid() const noexcept
{
    return socket_.valid();
}

int Connection::fd() const noexcept
{
    return socket_.fd();
}

ConnectionReadResult Connection::readAvailable()
{
    using netpulse::protocol::FrameDecodeStatus;

    ConnectionReadResult result{};

    constexpr std::size_t kReadBufferSize{4096};

    std::array<std::byte, kReadBufferSize>
        read_buffer{};

    while (true) {
        const auto received = ::recv(
            socket_.fd(),
            read_buffer.data(),
            read_buffer.size(),
            0);

        if (received > 0) {
            const auto received_size{
                static_cast<std::size_t>(received)
            };

            decoder_.append(
                std::span<const std::byte>{
                    read_buffer.data(),
                    received_size
                });

            while (true) {
                auto decode_result{
                    decoder_.decodeNext()
                };

                if (decode_result.status
                    == FrameDecodeStatus::frame_ready) {
                    result.frames.push_back(
                        std::move(
                            decode_result.payload));

                    continue;
                }

                if (decode_result.status
                    == FrameDecodeStatus::
                        payload_too_large) {
                    result.status =
                        ConnectionReadStatus::
                            protocol_error;

                    return result;
                }

                break;
            }

            continue;
        }

        if (received == 0) {
            result.status =
                ConnectionReadStatus::peer_closed;

            return result;
        }

        const int error_number{errno};

        if (error_number == EINTR) {
            continue;
        }

        if (error_number == EAGAIN
            || error_number == EWOULDBLOCK) {
            return result;
        }

        result.status =
            ConnectionReadStatus::io_error;

        result.error_number = error_number;
        return result;
    }
}

netpulse::protocol::FrameQueueStatus
Connection::queueFrame(
    std::span<const std::byte> payload)
{
    return write_buffer_.enqueue(payload);
}

netpulse::protocol::FrameFlushResult
Connection::flushWrites() noexcept
{
    return write_buffer_.flush(socket_.fd());
}

bool Connection::wantsWrite() const noexcept
{
    return !write_buffer_.empty();
}

}  // namespace netpulse::server