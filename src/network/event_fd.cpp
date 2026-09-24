#include "netpulse/network/event_fd.hpp"

#include <cerrno>
#include <cstddef>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>

namespace netpulse::network {

EventFd::EventFd() noexcept
    : EventFd{0}
{
}

EventFd::EventFd(
    unsigned int initial_value) noexcept
    : fd_{
          ::eventfd(
              initial_value,
              EFD_NONBLOCK | EFD_CLOEXEC)}
{
    if (fd_ < 0)
    {
        error_number_ = errno;
    }
}

EventFd::~EventFd()
{
    closeDescriptor();
}

EventFd::EventFd(EventFd&& other) noexcept
    : fd_{std::exchange(other.fd_, -1)},
      error_number_{
          std::exchange(other.error_number_, 0)}
{
}

EventFd& EventFd::operator=(
    EventFd&& other) noexcept
{
    if (this != &other)
    {
        closeDescriptor();

        fd_ = std::exchange(other.fd_, -1);
        error_number_ =
            std::exchange(other.error_number_, 0);
    }

    return *this;
}

bool EventFd::valid() const noexcept
{
    return fd_ >= 0;
}

int EventFd::fd() const noexcept
{
    return fd_;
}

int EventFd::errorNumber() const noexcept
{
    return error_number_;
}

EventFdOperationResult EventFd::notify(
    std::uint64_t value) const noexcept
{
    if (!valid())
    {
        return {
            false,
            EBADF
        };
    }

    if (value == 0)
    {
        return {
            false,
            EINVAL
        };
    }

    while (true)
    {
        const ssize_t bytes_written{
            ::write(
                fd_,
                &value,
                sizeof(value))
        };

        if (bytes_written ==
            static_cast<ssize_t>(sizeof(value)))
        {
            return {
                true,
                0
            };
        }

        if (bytes_written < 0 && errno == EINTR)
        {
            continue;
        }

        const int operation_error{
            bytes_written < 0 ? errno : EIO
        };

        return {
            false,
            operation_error
        };
    }
}

EventFdReadResult EventFd::consume() const noexcept
{
    if (!valid())
    {
        return {
            false,
            0,
            EBADF
        };
    }

    std::uint64_t value{0};

    while (true)
    {
        const ssize_t bytes_read{
            ::read(
                fd_,
                &value,
                sizeof(value))
        };

        if (bytes_read ==
            static_cast<ssize_t>(sizeof(value)))
        {
            return {
                true,
                value,
                0
            };
        }

        if (bytes_read < 0 && errno == EINTR)
        {
            continue;
        }

        const int operation_error{
            bytes_read < 0 ? errno : EIO
        };

        return {
            false,
            0,
            operation_error
        };
    }
}

void EventFd::closeDescriptor() noexcept
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace netpulse::network