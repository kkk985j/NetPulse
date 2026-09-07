#pragma once

#include <cstdint>
#include <span>
#include <sys/epoll.h>

namespace netpulse::network {

struct EpollOperationResult {
    bool success{false};
    int error_number{0};

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return success;
    }
};

struct EpollWaitResult {
    int event_count{-1};
    int error_number{0};

    [[nodiscard]] bool completed() const noexcept
    {
        return event_count >= 0;
    }
};

class Epoll {
public:
    Epoll() noexcept;
    ~Epoll();

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    Epoll(Epoll&& other) noexcept;
    Epoll& operator=(Epoll&& other) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] int fd() const noexcept;

    [[nodiscard]] EpollOperationResult add(
        int watched_fd,
        std::uint32_t events) noexcept;

    [[nodiscard]] EpollOperationResult modify(
        int watched_fd,
        std::uint32_t events) noexcept;

    [[nodiscard]] EpollOperationResult remove(
        int watched_fd) noexcept;

    [[nodiscard]] EpollWaitResult wait(
        std::span<epoll_event> events,
        int timeout_ms) noexcept;

private:
    int fd_{-1};
};

}  // namespace netpulse::network