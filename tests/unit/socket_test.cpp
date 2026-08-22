#include "netpulse/network/socket.hpp"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <sys/socket.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

using netpulse::network::Socket;

namespace {

bool isClosed(int fd)
{
    errno = 0;

    return ::fcntl(fd, F_GETFD) == -1
        && errno == EBADF;
}

int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

}  // namespace

int main()
{
    static_assert(!std::is_copy_constructible_v<Socket>);
    static_assert(!std::is_copy_assignable_v<Socket>);
    static_assert(std::is_nothrow_move_constructible_v<Socket>);
    static_assert(std::is_nothrow_move_assignable_v<Socket>);

    int first_pair[2]{-1, -1};
    int second_pair[2]{-1, -1};

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, first_pair) < 0) {
        return fail("could not create first socket pair");
    }

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, second_pair) < 0) {
        ::close(first_pair[0]);
        ::close(first_pair[1]);
        return fail("could not create second socket pair");
    }

    const int first_owned_fd = first_pair[0];
    const int second_owned_fd = second_pair[0];

    {
        Socket first{first_owned_fd};

        if (!first.valid() || first.fd() != first_owned_fd) {
            return fail("constructor did not take ownership");
        }

        Socket moved{std::move(first)};

        if (first.valid()) {
            return fail("move constructor left source valid");
        }

        if (!moved.valid() || moved.fd() != first_owned_fd) {
            return fail("move constructor did not transfer ownership");
        }

        Socket assigned{second_owned_fd};
        assigned = std::move(moved);

        if (moved.valid()) {
            return fail("move assignment left source valid");
        }

        if (!assigned.valid() || assigned.fd() != first_owned_fd) {
            return fail("move assignment did not transfer ownership");
        }

        if (!isClosed(second_owned_fd)) {
            return fail("move assignment did not close old descriptor");
        }
    }

    if (!isClosed(first_owned_fd)) {
        return fail("destructor did not close owned descriptor");
    }

    ::close(first_pair[1]);
    ::close(second_pair[1]);

    std::cout << "Socket RAII tests passed.\n";
    return 0;
}