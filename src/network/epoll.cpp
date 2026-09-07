#include "netpulse/network/epoll.hpp"

#include <cerrno>
#include <limits>
#include <unistd.h>
#include <utility>

namespace netpulse::network {

namespace {

EpollOperationResult controlEpoll(
    int epoll_fd,
    int operation,
    int watched_fd,
    std::uint32_t events) noexcept
{
    epoll_event event{};
    event.events = events;
    event.data.fd = watched_fd;

    epoll_event* event_pointer{
        operation == EPOLL_CTL_DEL
            ? nullptr
            : &event
    };

    if (::epoll_ctl(
            epoll_fd,
            operation,
            watched_fd,
            event_pointer) == -1) {
        const int error_number{errno};

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

}  // namespace

Epoll::Epoll() noexcept
    : fd_{::epoll_create1(EPOLL_CLOEXEC)}
{
}

Epoll::~Epoll()
{
    if (valid()) {
        ::close(fd_);
    }
}

Epoll::Epoll(Epoll&& other) noexcept
    : fd_{std::exchange(other.fd_, -1)}
{
}

Epoll& Epoll::operator=(Epoll&& other) noexcept
{
    if (this != &other) {
        if (valid()) {
            ::close(fd_);
        }

        fd_ = std::exchange(other.fd_, -1);
    }

    return *this;
}

bool Epoll::valid() const noexcept
{
    return fd_ >= 0;
}

int Epoll::fd() const noexcept
{
    return fd_;
}

EpollOperationResult Epoll::add(
    int watched_fd,
    std::uint32_t events) noexcept
{
    if (!valid() || watched_fd < 0) {
        return {
            false,
            EBADF
        };
    }

    return controlEpoll(
        fd_,
        EPOLL_CTL_ADD,
        watched_fd,
        events);
}

EpollOperationResult Epoll::modify(
    int watched_fd,
    std::uint32_t events) noexcept
{
    if (!valid() || watched_fd < 0) {
        return {
            false,
            EBADF
        };
    }

    return controlEpoll(
        fd_,
        EPOLL_CTL_MOD,
        watched_fd,
        events);
}

EpollOperationResult Epoll::remove(
    int watched_fd) noexcept
{
    if (!valid() || watched_fd < 0) {
        return {
            false,
            EBADF
        };
    }

    return controlEpoll(
        fd_,
        EPOLL_CTL_DEL,
        watched_fd,
        0);
}

EpollWaitResult Epoll::wait(
    std::span<epoll_event> events,
    int timeout_ms) noexcept
{
    if (!valid()) {
        return {
            -1,
            EBADF
        };
    }

    if (events.empty()
        || events.size()
            > static_cast<std::size_t>(
                std::numeric_limits<int>::max())) {
        return {
            -1,
            EINVAL
        };
    }

    int event_count{-1};

    do {
        event_count = ::epoll_wait(
            fd_,
            events.data(),
            static_cast<int>(events.size()),
            timeout_ms);
    } while (event_count == -1 && errno == EINTR);

    if (event_count == -1) {
        const int error_number{errno};

        return {
            -1,
            error_number
        };
    }

    return {
        event_count,
        0
    };
}

}  // namespace netpulse::network