#include "netpulse/network/io.hpp"

#include <cerrno>
#include <sys/socket.h>

namespace netpulse::network 
{
    IoResult sendAll (
        int fd,
        std::span<const std::byte> data
    ) noexcept
    {
        std::size_t total_sent{0};
        while (total_sent < data.size())
        {
            const auto sent = ::send(
                fd,
                data.data() + total_sent,
                data.size() - total_sent,
                MSG_NOSIGNAL
            );

            if(sent > 0) 
            {
                total_sent += static_cast<std::size_t> (sent);
                continue;
            }

            if(sent == 0)
            {
                return {
                    IoStatus::error,
                    total_sent,
                    EPIPE
                };
            }

            const int error_number = errno;

            if(error_number == EINTR)
            {
                continue;
            }

            if(error_number == EAGAIN || error_number == EWOULDBLOCK)
            {
                return {
                    IoStatus::would_block,
                    total_sent,
                    error_number
                };
            }

            return {
                IoStatus::error,
                total_sent,
                error_number
            };
        }

        return {
            IoStatus::completed,
            total_sent,
            0
        };
    }

    IoResult receiveExact(
        int fd,
        std::span<std::byte> buffer
    ) noexcept
    {
        std::size_t total_received{0};

        while(total_received < buffer.size())
        {
            const auto received = ::recv(
                fd,
                buffer.data() + total_received,
                buffer.size() - total_received,
                0
            );

            if(received > 0) 
            {
                total_received += static_cast<std::size_t> (received);
                continue;
            }

            if(received == 0)
            {
                return {
                    IoStatus::peer_closed,
                    total_received,
                    0
                };
            }

            const int error_number = errno;

            if(error_number == EINTR)
            {
                continue;
            }

            if(error_number == EAGAIN || error_number == EWOULDBLOCK)
            {
                return {
                    IoStatus::would_block,
                    total_received,
                    error_number
                };
            }

            return {
                IoStatus::error,
                total_received,
                error_number
            };
        }

        return {
            IoStatus::completed,
            total_received,
            0
        };
    }
}
// namespace netpulse::network