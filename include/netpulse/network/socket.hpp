#pragma once

namespace netpulse::network {

class Socket {
public:
    Socket() noexcept = default;
    explicit Socket(int fd) noexcept;

    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    [[nodiscard]] int fd() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] int release() noexcept;
    void reset(int new_fd = -1) noexcept;

private:
    int fd_{-1};
};

}  // namespace netpulse::network