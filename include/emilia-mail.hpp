#pragma once

#define EMILIA_MAIL_VERSION_MAJOR 2
#define EMILIA_MAIL_VERSION_MINOR 1
#define EMILIA_MAIL_VERSION_REVISION 0

#undef UNICODE

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

int main(int, const char *[]);
