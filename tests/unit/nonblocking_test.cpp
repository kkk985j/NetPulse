#include "netpulse/network/nonblocking.hpp"
#include "netpulse/network/socket.hpp"

#include <fcntl.h>
#include <iostream>
#include <sys/socket.h>

using netpulse::network::Socket;
using netpulse::network::setNonBlocking;

namespace {

int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

}  // namespace

int main()
{
    int socket_pair[2]{-1, -1};

    if (::socketpair(
            AF_UNIX,
            SOCK_STREAM,
            0,
            socket_pair) < 0) {
        return fail("could not create socket pair");
    }

    Socket first_socket{socket_pair[0]};
    Socket second_socket{socket_pair[1]};

    const int flags_before{
        ::fcntl(first_socket.fd(), F_GETFL, 0)
    };

    if (flags_before == -1) {
        return fail("could not read initial flags");
    }

    const auto result = setNonBlocking(
        first_socket.fd());

    if (!result) {
        return fail("setNonBlocking failed");
    }

    const int flags_after{
        ::fcntl(first_socket.fd(), F_GETFL, 0)
    };

    if (flags_after == -1) {
        return fail("could not read updated flags");
    }

    if ((flags_after & O_NONBLOCK) == 0) {
        return fail("O_NONBLOCK was not enabled");
    }

    if ((flags_after & ~O_NONBLOCK)
        != (flags_before & ~O_NONBLOCK)) {
        return fail("existing flags were changed");
    }

    const auto repeated_result = setNonBlocking(
        first_socket.fd());

    if (!repeated_result) {
        return fail("repeated call should succeed");
    }

    const auto invalid_result = setNonBlocking(-1);

    if (invalid_result) {
        return fail("invalid descriptor was accepted");
    }

    if (invalid_result.error_number == 0) {
        return fail("invalid descriptor did not report errno");
    }

    std::cout << "Non-blocking socket tests passed.\n";
    return 0;
}