#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <array>
#include <string_view>
#include "netpulse/network/socket.hpp"
using netpulse::network::Socket;

namespace {
	constexpr int kServerPort = 9000;
	constexpr int kListenBacklog = 16;
}

int main()
{
	Socket server_socket{
		::socket(AF_INET, SOCK_STREAM, 0);
	}

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

	std::array<char,1024> buffer{};

	const ssize_t bytes_received = ::recv(
		client_socket.fd(),
		buffer.data(),
		buffer.size(),
		0
	);

	if (bytes_received < 0)
	{
		std::cerr << "recv() failed: "
			<< std::strerror(errno) << std::endl;


		return 1;
	}

	if (bytes_received == 0)
	{
		std::cout << "Client disconnetded without sending data.\n";
	}else
	{
		std::cout << "Received " << bytes_received << " bytes: ";

		std::cout.write(
			buffer.data(),
			static_cast <std::streamsize> (bytes_received)
		);

		std::cout << std::endl;

		constexpr std::string_view response = "ACK from NetPulse!\n";

		const ssize_t bytes_sent = ::send(
			client_socket.fd(),
			response.data(),
			response.size(),
			0
		);

		if (bytes_sent < 0)
		{
			std::cerr << "send() failed: "
				<< std::strerror(errno) << std::endl;


			return 1;
		}

		if (bytes_sent != static_cast<ssize_t> (response.size()))
		{
			std::cerr << "send() transmitted only part of the response.\n";


			return 1;
		}

		std::cout << "ACK sent to client.\n";
	}


	return 0;
}