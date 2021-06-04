#pragma once

#define EMILIA_MAIL_VERSION_MAJOR 2
#define EMILIA_MAIL_VERSION_MINOR 2
#define EMILIA_MAIL_VERSION_REVISION 10

#include "build.hpp"

#include <rain.hpp>

class Slave : public Rain::Networking::Smtp::Slave {
	public:
	static const std::size_t BUF_SZ = 16384;

	char *buf;
	bool authenticated;
	std::string mailFrom, data;
	std::set<std::string> rcptTo;

	Slave(Rain::Networking::Socket &socket,
		std::size_t BUF_SZ = 16384,
		const std::chrono::milliseconds &RECV_TIMEOUT_MS =
			std::chrono::milliseconds(5000),
		const std::chrono::milliseconds &SEND_MS_PER_KB = std::chrono::milliseconds(
			5000),
		const std::chrono::milliseconds &SEND_TIMEOUT_MS_LOWER =
			std::chrono::milliseconds(5000));
	~Slave();
};

class Server : public Rain::Networking::Smtp::Server<Slave> {
	protected:
	bool onRequest(Slave &slave, Request &req) noexcept override;
};

int main(int, const char *[]);
