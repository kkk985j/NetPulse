#include "netpulse/network/nonblocking.hpp"

#include <cerrno>
#include <fcntl.h>

namespace netpulse::network {

    NonBlockingResult setNonBlocking (int fd) noexcept
    {
        const int current_flags
        {
            ::fcntl(fd , F_GETFL , 0)
        };

        if (current_flags == -1)
        {
            const int error_number {errno};

            return {
                false,
                error_number
            };
        }

        if ((current_flags & O_NONBLOCK) != 0)
        {
            return {
                true,
                0
            };
        }

        if (::fcntl(fd,F_SETFL,current_flags | O_NONBLOCK) == -1)
        {
            const int error_number {errno};

            return {
                false,
                error_number
            };
        }

        return {
            true,
            0
        };
    }
}