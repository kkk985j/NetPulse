#include "netpulse/network/socket.hpp"
#include "netpulse/protocol/frame.hpp"

#include <arpa/inet.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <span>
#include <string_view>
#include <sys/socket.h>

using netpulse::network::Socket;
using netpulse::protocol::receiveFrame;
using netpulse::protocol::sendFrame;

namespace {

constexpr int kServerPort{9000};
constexpr std::string_view kServerAddress{
    "127.0.0.1"
};

}  // namespace

int main()
{
    std::cout
        << "NetPulse Device Simulator v0.1.0 starting...\n";

    Socket connection{
        ::socket(AF_INET, SOCK_STREAM, 0)
    };

    if (!connection.valid()) {
        std::cerr
            << "Failed to create socket: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(kServerPort);

    if (::inet_pton(
            AF_INET,
            kServerAddress.data(),
            &server_address.sin_addr) != 1) {
        std::cerr << "Invalid server address.\n";
        return 1;
    }

    if (::connect(
            connection.fd(),
            reinterpret_cast<sockaddr*>(&server_address),
            sizeof(server_address)) < 0) {
        std::cerr
            << "Failed to connect to server: "
            << std::strerror(errno)
            << '\n';

        return 1;
    }

    std::cout << "Connected to NetPulse server.\n";

    constexpr std::string_view telemetry{
        R"({"device_id":"sensor-001","temperature":25.4,"humidity":61})"
    };

    const auto telemetry_bytes = std::as_bytes(
        std::span{
            telemetry.data(),
            telemetry.size()
        });

    const auto send_result = sendFrame(
        connection.fd(),
        telemetry_bytes);

    if (!send_result.completed()) {
        std::cerr
            << "sendFrame() failed after "
            << send_result.bytes_transferred
            << " bytes, error number: "
            << send_result.error_number
            << '\n';

        return 1;
    }

    std::cout
        << "Telemetry frame sent: "
        << telemetry
        << '\n';

    const auto response_result = receiveFrame(
        connection.fd());

    if (!response_result.completed()) {
        std::cerr
            << "receiveFrame() failed after "
            << response_result.bytes_transferred
            << " bytes, error number: "
            << response_result.error_number
            << '\n';

        return 1;
    }

    std::cout << "Server response: ";

    if (!response_result.payload.empty()) {
        std::cout.write(
            reinterpret_cast<const char*>(
                response_result.payload.data()),
            static_cast<std::streamsize>(
                response_result.payload.size()));
    }

    std::cout << '\n';
    return 0;
}