#include "netpulse/network/socket.hpp"

#include <unistd.h>

#include <utility>

namespace netpulse::network {

Socket::Socket(int fd) noexcept
    : fd_(fd)
{
}

Socket::~Socket()
{
    reset();
}

Socket::Socket(Socket&& other) noexcept
    : fd_(other.release())
{
}

Socket& Socket::operator=(Socket&& other) noexcept
{
    if (this != &other) {
        reset(other.release());
    }

    return *this;
}

int Socket::fd() const noexcept
{
    return fd_;
}

bool Socket::valid() const noexcept
{
    return fd_ >= 0;
}

int Socket::release() noexcept
{
    return std::exchange(fd_, -1);
}

void Socket::reset(int new_fd) noexcept
{
    if (fd_ == new_fd) {
        return;
    }

    if (valid()) {
        ::close(fd_);
    }

    fd_ = new_fd;
}

}  // namespace netpulse::network