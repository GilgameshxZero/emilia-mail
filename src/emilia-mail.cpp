#include "emilia-mail.hpp"

int main(int argc, const char *argv[]) {
	const std::string EMILIA_MAIL_VERSION_STR =
											std::to_string(EMILIA_MAIL_VERSION_MAJOR) + "." +
		std::to_string(EMILIA_MAIL_VERSION_MINOR) + "." +
		std::to_string(EMILIA_MAIL_VERSION_REVISION) + "." +
		std::to_string(EMILIA_MAIL_VERSION_BUILD),
										RAIN_VERSION_STR = std::to_string(RAIN_VERSION_MAJOR) +
		"." + std::to_string(RAIN_VERSION_MINOR) + "." +
		std::to_string(RAIN_VERSION_REVISION) + "." +
		std::to_string(RAIN_VERSION_BUILD);
	std::cout << "emilia-mail " << EMILIA_MAIL_VERSION_STR << "\n"
						<< "using rain " << RAIN_VERSION_STR << "\n";

	// Parse command line.
	// Forwarding address and password need to be set.
	std::string port = "25", forwardingAddress = "example@example.com",
							password = "password";
	std::vector<std::string> domains = {"gilgamesh.cc"};
	Rain::String::CommandLineParser parser;
	parser.addLayer("port", &port);
	parser.addLayer("forwarding-address", &forwardingAddress);
	parser.addLayer("password", &password);
	parser.addLayer("D", &domains);
	parser.addLayer("domain", &domains);
	parser.parse(argc - 1, argv + 1);

	std::set<std::string> domainsSet(domains.begin(), domains.end());

	// Helper functions.
	std::function<std::string(const std::string &)> extractEmailDomain =
		[](const std::string &email) {
			char *atSymbol = const_cast<char *>(email.c_str());
			for (; *atSymbol != '\0' && *atSymbol != '@'; atSymbol++)
				;
			return std::string(email.c_str() + (atSymbol - email.c_str()) + 1);
		};

	// Run server.
	std::mutex coutMtx;
	std::atomic<std::size_t> cSlaves;
	typedef Rain::Networking::Smtp::Server<ServerSlaveData> Server;
	Server server(512);
	server.onBeginSlaveTask = [&](Server::Slave *slave) {
		Rain::Networking::Smtp::Response res(220, "emilia ready");
		slave->send(&res);
		std::lock_guard<std::mutex> coutLck(coutMtx);
		std::cout << "Began new slave task: " << ++cSlaves << "\n";
	};
	server.onCloseSlave = [&](Server::Slave *slave) {
		std::lock_guard<std::mutex> coutLck(coutMtx);
		std::cout << "Ended slave task: " << --cSlaves << "\n";
	};
	server.onRequest = [&](Server::Request *req) {
		Server::Response res(req->slave);
		if (req->verb == "EHLO" || req->verb == "HELO") {
			res.code = 250;
			res.parameter = domains[0];
			res.extensions.emplace_back(std::vector<std::string>({"AUTH", "LOGIN"}));
			req->slave->send(&res);
		} else if (req->verb == "DATA") {
			static const char *CRLF_DOT_CRLF = "\r\n.\r\n";
			static const std::size_t PART_MATCH_CRLF_DOT_CRLF[] = {
				(std::numeric_limits<std::size_t>::max)(),
				0,
				0,
				(std::numeric_limits<std::size_t>::max)(),
				0,
				2};

			// Either sender or recipient must be at a managed domain.
			bool unrelatedDomain = domainsSet.find(extractEmailDomain(
															 req->slave->data.mailFrom)) == domainsSet.end();
			for (auto it : req->slave->data.rcptTo) {
				unrelatedDomain = unrelatedDomain &&
					domainsSet.find(extractEmailDomain(it)) == domainsSet.end();
			}
			if (unrelatedDomain) {
				res.code = 510;
				res.parameter = "emilia doesn't speak for strangers";
				req->slave->send(&res);
				return;
			} else {
				res.code = 354;
				res.parameter = "ready";
				req->slave->send(&res);
			}

			// Receive data.
			std::size_t kmpCand = 0, recvLen;
			do {
				recvLen = req->slave->recv(req->slave->data.buffer,
					ServerSlaveData::BUF_SZ,
					Rain::Networking::Socket::RecvFlag::NONE,
					req->slave->server->recvTimeoutMs);
				if (recvLen == 0) {	 // Unintended exit.
					res.code = 500;
					res.parameter = "unsupported format";
					req->slave->send(&res);
					return;
				}
				req->slave->data.data.append(req->slave->data.buffer, recvLen);
			} while (Rain::Algorithm::cStrSearchKMP(req->slave->data.buffer,
								 recvLen,
								 CRLF_DOT_CRLF,
								 5,
								 PART_MATCH_CRLF_DOT_CRLF,
								 &kmpCand) == NULL);

			// Relay all mail data to correct external SMTP client. Only return 250 if
			// successful.
			bool someFailed = false;
			for (auto it : req->slave->data.rcptTo) {
				std::string domain = extractEmailDomain(it), to = it;
				if (domainsSet.find(domain) != domainsSet.end()) {
					to = forwardingAddress;
					domain = extractEmailDomain(to);
				}
				Rain::Networking::Smtp::Client client;
				int result;
				if ((result = client.connectDomain(domain))) {
					std::lock_guard<std::mutex> coutLck(coutMtx);
					std::cout << "[" << req->slave->getNativeSocket()
										<< "] Failed to connect to SMTP server at " << domain
										<< " with error code " << result << "\n";
					someFailed = true;
				} else if ((result = client.sendEmail(domains[0],
											req->slave->data.mailFrom,
											to,
											req->slave->data.data))) {
					std::lock_guard<std::mutex> coutLck(coutMtx);
					std::cout << "[" << req->slave->getNativeSocket()
										<< "] Failed to send email from "
										<< req->slave->data.mailFrom << " to " << to
										<< " originally intended for " << it << " with error code "
										<< result << "\n";
					someFailed = true;
				} else {
					std::lock_guard<std::mutex> coutLck(coutMtx);
					std::cout << "[" << req->slave->getNativeSocket()
										<< "] Sent email to " << to << "\n";
				}
			}

			if (someFailed) {
				res.code = 554;
				res.parameter = "emilia failed to relay email data";
				req->slave->send(&res);
				std::lock_guard<std::mutex> coutLck(coutMtx);
				std::cout << req->slave->data.data;
			} else {
				res.code = 250;
				res.parameter = "OK";
				req->slave->send(&res);
			}
		} else if (req->verb == "AUTH" && req->parameter == "LOGIN") {
			static const char *CRLF = "\r\n";
			static const std::size_t PART_MATCH_CRLF[] = {
				(std::numeric_limits<std::size_t>::max)(), 0, 0};

			// Receive username.
			res.code = 334;
			res.parameter = "VXNlcm5hbWU6";
			req->slave->send(&res);
			std::size_t kmpCand = 0, recvLen;
			std::string usernameB64, passwordB64;
			do {
				recvLen = req->slave->recv(req->slave->data.buffer,
					ServerSlaveData::BUF_SZ,
					Rain::Networking::Socket::RecvFlag::NONE,
					req->slave->server->recvTimeoutMs);
				if (recvLen == 0) {	 // Unintended exit.
					res.code = 500;
					res.parameter = "unsupported format";
					req->slave->send(&res);
					req->slave->shutdown();
					return;
				}
				usernameB64.append(req->slave->data.buffer, recvLen);
			} while (Rain::Algorithm::cStrSearchKMP(req->slave->data.buffer,
								 recvLen,
								 CRLF,
								 2,
								 PART_MATCH_CRLF,
								 &kmpCand) == NULL);
			usernameB64.pop_back();
			usernameB64.pop_back();

			// Receive password.
			res.code = 334;
			res.parameter = "UGFzc3dvcmQ6";
			req->slave->send(&res);
			kmpCand = 0;
			do {
				recvLen = req->slave->recv(req->slave->data.buffer,
					ServerSlaveData::BUF_SZ,
					Rain::Networking::Socket::RecvFlag::NONE,
					req->slave->server->recvTimeoutMs);
				if (recvLen == 0) {	 // Unintended exit.
					res.code = 500;
					res.parameter = "unsupported format";
					req->slave->send(&res);
					req->slave->shutdown();
					return;
				}
				passwordB64.append(req->slave->data.buffer, recvLen);
			} while (Rain::Algorithm::cStrSearchKMP(req->slave->data.buffer,
								 recvLen,
								 CRLF,
								 2,
								 PART_MATCH_CRLF,
								 &kmpCand) == NULL);
			passwordB64.pop_back();
			passwordB64.pop_back();

			// Verify password.
			if (passwordB64 == password) {
				req->slave->data.authenticated = true;
				res.code = 235;
				res.parameter = "emilia has authenticated you";
				req->slave->send(&res);
			} else {
				res.code = 530;
				res.parameter = "emilia could not authenticate you";
				req->slave->send(&res);
			}
		} else if (req->verb == "MAIL") {
			char *leftBracket = const_cast<char *>(req->parameter.c_str()),
					 *rightBracket;
			for (; *leftBracket != '\0' && *leftBracket != '<'; leftBracket++)
				;
			for (rightBracket = leftBracket;
					 *rightBracket != '\0' && *rightBracket != '>';
					 rightBracket++)
				;
			Rain::String::toLowerStr(&req->slave->data.mailFrom.assign(
				leftBracket + 1, rightBracket - leftBracket - 1));

			// Verify authentication if sending from managed domain.
			if (domainsSet.find(req->slave->data.mailFrom) != domainsSet.end() &&
				!req->slave->data.authenticated) {
				res.code = 502;
				res.parameter = "emilia doesn't trust you yet";
				req->slave->send(&res);
			} else {
				res.code = 250;
				res.parameter = "OK";
				req->slave->send(&res);
			}
		} else if (req->verb == "RCPT") {
			char *leftBracket = const_cast<char *>(req->parameter.c_str()),
					 *rightBracket;
			for (; *leftBracket != '\0' && *leftBracket != '<'; leftBracket++)
				;
			for (rightBracket = leftBracket;
					 *rightBracket != '\0' && *rightBracket != '>';
					 rightBracket++)
				;
			std::string address(leftBracket + 1, rightBracket - leftBracket - 1);
			req->slave->data.rcptTo.insert(*Rain::String::toLowerStr(&address));
			res.code = 250;
			res.parameter = "OK";
			req->slave->send(&res);
		} else if (req->verb == "RSET") {
			req->slave->data = ServerSlaveData();
			res.code = 250;
			req->slave->send(&res);
		} else if (req->verb == "QUIT") {
			res.code = 221;
			res.parameter = "OK";
			req->slave->send(&res);
		} else {
			res.code = 500;
			res.parameter = "unrecognized command";
			req->slave->send(&res);
		}
	};

	server.serve(Rain::Networking::Host(NULL, port), false);
	std::cout << "Serving on port " << server.getService().getCStr() << ".\n";
	std::string command;
	while (command != "exit") {
		std::cin >> command;
	}

	return 0;
}
