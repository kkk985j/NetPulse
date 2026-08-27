#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>

#include <string_view>
#include <span>

#include "netpulse/network/socket.hpp"
#include "netpulse/protocol/frame.hpp"

using netpulse::network::Socket;
using netpulse::protocol::FrameStatus;
using netpulse::protocol::receiveFrame;
using netpulse::protocol::sendFrame;

namespace {
	constexpr int kServerPort = 9000;
	constexpr int kListenBacklog = 16;
}

int main()
{
	Socket server_socket{
		::socket(AF_INET, SOCK_STREAM, 0)
	};

	if (!server_socket.valid())
	{
		std::cerr << "Failed to create socket: "
			<< std::strerror(errno) << std::endl;
		return 1;
	}

	int reuse_address = 1;

	if (::setsockopt(
			server_socket.fd(),
			SOL_SOCKET,
			SO_REUSEADDR,
			&reuse_address,
			sizeof(reuse_address)) < 0)
	{
		std::cerr << "Failed to set socket options: "
			<< std::strerror(errno) << std::endl;
		return 1;
	}

	sockaddr_in server_address{};
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(kServerPort);

	if	(::bind(
			server_socket.fd(),
			reinterpret_cast<sockaddr*>(&server_address),
			sizeof(server_address)) < 0)
	{
		std::cerr << "Failed to bind socket: "
			<< std::strerror(errno) << std::endl;


		return 1;
	}

	if (::listen(server_socket.fd(), kListenBacklog) < 0)
	{
		std::cerr << "Failed to listen on socket: "
			<< std::strerror(errno) << std::endl;


		return 1;
	}

	std::cout << "NetPulse server listening on 0.0.0.0:"
		<< kServerPort << std::endl;

	sockaddr_in client_address{};
	socklen_t client_address_length = sizeof(client_address);

	Socket client_socket{
    ::accept(
        server_socket.fd(),
        reinterpret_cast<sockaddr*>(&client_address),
        &client_address_length)
	};

	if (!client_socket.valid())
	{
		std::cerr << "accept() failed: "
			<< std::strerror(errno) << std::endl;


		return 1;
	}
	//add function: accpet & reply
	// std::cout << "Client connected.\n";



	std::cout << "Client connected.\n";

    const auto receive_result = receiveFrame(
        client_socket.fd());

    if (!receive_result.completed()) {
        if (receive_result.status
            == FrameStatus::peer_closed) {
            std::cout
                << "Client disconnected before completing a frame.\n";
            return 0;
        }

        if (receive_result.status
            == FrameStatus::payload_too_large) {
            std::cerr
                << "Client sent an oversized frame.\n";
            return 1;
        }

        std::cerr
            << "receiveFrame() failed after "
            << receive_result.bytes_transferred
            << " bytes, error number: "
            << receive_result.error_number
            << '\n';

        return 1;
    }

    std::cout
        << "Received "
        << receive_result.payload.size()
        << " payload bytes: ";

    if (!receive_result.payload.empty()) {
        std::cout.write(
            reinterpret_cast<const char*>(
                receive_result.payload.data()),
            static_cast<std::streamsize>(
                receive_result.payload.size()));
    }

    std::cout << '\n';

    constexpr std::string_view response{
        "ACK from NetPulse!"
    };

    const auto response_bytes = std::as_bytes(
        std::span{
            response.data(),
            response.size()
        });

    const auto send_result = sendFrame(
        client_socket.fd(),
        response_bytes);

    if (!send_result.completed()) {
        std::cerr
            << "sendFrame() failed after "
            << send_result.bytes_transferred
            << " bytes, error number: "
            << send_result.error_number
            << '\n';

        return 1;
    }

    std::cout << "ACK frame sent to client.\n";


	return 0;
}