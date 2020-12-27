#pragma once

#define EMILIA_MAIL_VERSION_MAJOR 2
#define EMILIA_MAIL_VERSION_MINOR 2
#define EMILIA_MAIL_VERSION_REVISION 1

#include "build.hpp"

#include <rain.hpp>

class ServerSlaveData {
	public:
	static const std::size_t BUF_SZ = 16384;

	char buffer[BUF_SZ];
	bool authenticated;
	std::string mailFrom, data;
	std::set<std::string> rcptTo;
};

class Server;

class ServerSlave : public Rain::Networking::Smtp::
											ServerSlave<Server, ServerSlave, ServerSlaveData> {
	public:
	typedef Rain::Networking::Smtp::
		ServerSlave<Server, ServerSlave, ServerSlaveData>
			ServerSlaveBase;
	ServerSlave(Rain::Networking::Socket &socket, Server *server)
			: Rain::Networking::Socket(std::move(socket)),
				ServerSlaveBase(socket, server) {}
};

class Server : public Rain::Networking::Smtp::Server<ServerSlave> {
	public:
	typedef Rain::Networking::Smtp::Server<ServerSlave> ServerBase;
	Server(std::size_t maxThreads = 0, std::size_t slaveBufSz = 16384)
			: ServerBase(maxThreads, slaveBufSz) {}

	protected:
	void *getSubclassPtr() { return reinterpret_cast<void *>(this); }
	void onBeginSlaveTask(Slave *);
	void onRequest(Request *);
};

int main(int, const char *[]);
