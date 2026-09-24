#pragma once

#include <cstdint>

namespace netpulse::network {

struct EventFdOperationResult
{
    bool success{false};
    int error_number{0};

    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return success;
    }
};

struct EventFdReadResult
{
    bool success{false};
    std::uint64_t value{0};
    int error_number{0};

    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return success;
    }
};

class EventFd
{
public:
    EventFd() noexcept;

    explicit EventFd(
        unsigned int initial_value) noexcept;

    ~EventFd();

    EventFd(const EventFd&) = delete;
    EventFd& operator=(const EventFd&) = delete;

    EventFd(EventFd&& other) noexcept;
    EventFd& operator=(EventFd&& other) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] int fd() const noexcept;
    [[nodiscard]] int errorNumber() const noexcept;

    [[nodiscard]] EventFdOperationResult notify(
        std::uint64_t value = 1) const noexcept;

    [[nodiscard]] EventFdReadResult consume()
        const noexcept;

private:
    void closeDescriptor() noexcept;

    int fd_{-1};
    int error_number_{0};
};

} // namespace netpulse::network