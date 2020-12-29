#include "emilia-mail.hpp"

// TODO: Refactor globals.
std::string port = "0", forwardingAddress = "example@example.com",
						password = "password";
std::vector<std::string> domains = {"gilgamesh.cc"};
std::set<std::string> domainsSet;
std::mutex coutMtx;

// Helper functions.
std::function<std::string(const std::string &)> extractEmailDomain =
	[](const std::string &email) {
		char *atSymbol = const_cast<char *>(email.c_str());
		for (; *atSymbol != '\0' && *atSymbol != '@'; atSymbol++)
			;
		return std::string(email.c_str() + (atSymbol - email.c_str()) + 1);
	};

Slave::Slave(Rain::Networking::Socket &socket,
	const std::chrono::milliseconds &RECV_TIMEOUT_MS,
	std::size_t BUF_SZ)
		: Rain::Networking::Socket(std::move(socket)),
			Rain::Networking::RequestResponse::Socket<Rain::Networking::Smtp::Request,
				Rain::Networking::Smtp::Response>(socket, RECV_TIMEOUT_MS, BUF_SZ),
			Rain::Networking::Smtp::Socket(socket, RECV_TIMEOUT_MS, BUF_SZ),
			Rain::Networking::Smtp::Slave(socket, RECV_TIMEOUT_MS, BUF_SZ) {
	this->buf = new char[this->BUF_SZ];
	Rain::Networking::Smtp::Response res(220, "emilia ready");
	this->send(res);
}
Slave::~Slave() {
	delete[] this->buf;
}

bool Server::onRequest(Slave &slave, Request &req) noexcept {
	try {
		Server::Response res;
		if (req.verb == "EHLO" || req.verb == "HELO") {
			res.code = 250;
			res.parameter = domains[0];
			res.extensions.emplace_back(std::vector<std::string>({"AUTH", "LOGIN"}));
			slave.send(res);
		} else if (req.verb == "DATA") {
			static const char *CRLF_DOT_CRLF = "\r\n.\r\n";
			static const std::size_t PART_MATCH_CRLF_DOT_CRLF[] = {
				(std::numeric_limits<std::size_t>::max)(),
				0,
				0,
				(std::numeric_limits<std::size_t>::max)(),
				0,
				2};

			// Either sender or recipient must be at a managed domain.
			bool unrelatedDomain =
				domainsSet.find(extractEmailDomain(slave.mailFrom)) == domainsSet.end();
			for (auto it : slave.rcptTo) {
				unrelatedDomain = unrelatedDomain &&
					domainsSet.find(extractEmailDomain(it)) == domainsSet.end();
			}
			if (unrelatedDomain) {
				res.code = 510;
				res.parameter = "emilia doesn't speak for strangers";
				slave.send(res);
				return false;
			} else {
				res.code = 354;
				res.parameter = "ready";
				slave.send(res);
			}

			// Receive data.
			std::size_t kmpCand = 0, recvLen;
			do {
				recvLen = slave.recv(slave.buf,
					slave.BUF_SZ,
					Rain::Networking::Socket::RecvFlag::NONE,
					slave.RECV_TIMEOUT_MS);
				if (recvLen == 0) {	 // Unintended exit.
					res.code = 500;
					res.parameter = "unsupported format";
					slave.send(res);
					return false;
				}
				slave.data.append(slave.buf, recvLen);
			} while (Rain::Algorithm::cStrSearchKMP(slave.buf,
								 recvLen,
								 CRLF_DOT_CRLF,
								 5,
								 PART_MATCH_CRLF_DOT_CRLF,
								 &kmpCand) == NULL);

			// Relay all mail data to correct external SMTP client. Only return 250 if
			// successful.
			bool someFailed = false;
			for (auto it : slave.rcptTo) {
				std::string domain = extractEmailDomain(it), to = it;
				if (domainsSet.find(domain) != domainsSet.end()) {
					to = forwardingAddress;
					domain = extractEmailDomain(to);
				}
				Rain::Networking::Smtp::Client client;
				int result;
				if ((result = client.connectDomain(domain))) {
					std::lock_guard<std::mutex> coutLck(coutMtx);
					std::cout << "[" << slave.getNativeSocket()
										<< "] Failed to connect to SMTP server at " << domain
										<< " with error code " << result << ".\n";
					someFailed = true;
				} else if ((result = client.sendEmail(
											domains[0], slave.mailFrom, to, slave.data))) {
					std::lock_guard<std::mutex> coutLck(coutMtx);
					std::cout << "[" << slave.getNativeSocket()
										<< "] Failed to send email from " << slave.mailFrom
										<< " to " << to << " originally intended for " << it
										<< " with error code " << result << ".\n";
					someFailed = true;
				}
			}

			if (someFailed) {
				res.code = 450;
				res.parameter = "emilia failed to relay email data, please try again";
				slave.send(res);

				std::string filename = std::to_string(
					std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt";
				std::ofstream out(filename, std::ios::binary);
				out.write(slave.data.c_str(), slave.data.length());
				out.close();

				std::lock_guard<std::mutex> coutLck(coutMtx);
				std::cout << "[" << slave.getNativeSocket()
									<< "] Failed to relay email. Data saved to " << filename
									<< "\n";
			} else {
				res.code = 250;
				res.parameter = "OK";
				slave.send(res);
			}
		} else if (req.verb == "AUTH" && req.parameter == "LOGIN") {
			static const char *CRLF = "\r\n";
			static const std::size_t PART_MATCH_CRLF[] = {
				(std::numeric_limits<std::size_t>::max)(), 0, 0};

			// Receive username.
			res.code = 334;
			res.parameter = "VXNlcm5hbWU6";
			slave.send(res);
			std::size_t kmpCand = 0, recvLen;
			std::string usernameB64, passwordB64;
			do {
				recvLen = slave.recv(slave.buf,
					slave.BUF_SZ,
					Rain::Networking::Socket::RecvFlag::NONE,
					slave.RECV_TIMEOUT_MS);
				if (recvLen == 0) {	 // Unintended exit.
					res.code = 500;
					res.parameter = "unsupported format";
					slave.send(res);
					return false;
				}
				usernameB64.append(slave.buf, recvLen);
			} while (
				Rain::Algorithm::cStrSearchKMP(
					slave.buf, recvLen, CRLF, 2, PART_MATCH_CRLF, &kmpCand) == NULL);
			usernameB64.pop_back();
			usernameB64.pop_back();

			// Receive password.
			res.code = 334;
			res.parameter = "UGFzc3dvcmQ6";
			slave.send(res);
			kmpCand = 0;
			do {
				recvLen = slave.recv(slave.buf,
					slave.BUF_SZ,
					Rain::Networking::Socket::RecvFlag::NONE,
					slave.RECV_TIMEOUT_MS);
				if (recvLen == 0) {	 // Unintended exit.
					res.code = 500;
					res.parameter = "unsupported format";
					slave.send(res);
					return false;
				}
				passwordB64.append(slave.buf, recvLen);
			} while (
				Rain::Algorithm::cStrSearchKMP(
					slave.buf, recvLen, CRLF, 2, PART_MATCH_CRLF, &kmpCand) == NULL);
			passwordB64.pop_back();
			passwordB64.pop_back();

			// Verify password.
			if (passwordB64 == password) {
				slave.authenticated = true;
				res.code = 235;
				res.parameter = "emilia has authenticated you";
				slave.send(res);
			} else {
				res.code = 530;
				res.parameter = "emilia could not authenticate you";
				slave.send(res);
			}
		} else if (req.verb == "MAIL") {
			char *leftBracket = const_cast<char *>(req.parameter.c_str()),
					 *rightBracket;
			for (; *leftBracket != '\0' && *leftBracket != '<'; leftBracket++)
				;
			for (rightBracket = leftBracket;
					 *rightBracket != '\0' && *rightBracket != '>';
					 rightBracket++)
				;
			Rain::String::toLowerStr(&slave.mailFrom.assign(
				leftBracket + 1, rightBracket - leftBracket - 1));

			// Verify authentication if sending from managed domain.
			if (domainsSet.find(slave.mailFrom) != domainsSet.end() &&
				!slave.authenticated) {
				res.code = 502;
				res.parameter = "emilia doesn't trust you yet";
				slave.send(res);
			} else {
				res.code = 250;
				res.parameter = "OK";
				slave.send(res);
			}
		} else if (req.verb == "RCPT") {
			char *leftBracket = const_cast<char *>(req.parameter.c_str()),
					 *rightBracket;
			for (; *leftBracket != '\0' && *leftBracket != '<'; leftBracket++)
				;
			for (rightBracket = leftBracket;
					 *rightBracket != '\0' && *rightBracket != '>';
					 rightBracket++)
				;
			std::string address(leftBracket + 1, rightBracket - leftBracket - 1);
			slave.rcptTo.insert(*Rain::String::toLowerStr(&address));
			res.code = 250;
			res.parameter = "OK";
			slave.send(res);
		} else if (req.verb == "RSET") {
			slave.authenticated = false;
			slave.mailFrom = std::string();
			slave.data = std::string();
			slave.rcptTo = std::set<std::string>();
			res.code = 250;
			slave.send(res);
		} else if (req.verb == "QUIT") {
			res.code = 221;
			res.parameter = "OK";
			slave.send(res);
		} else {
			res.code = 500;
			res.parameter = "unrecognized command";
			slave.send(res);
		}
		return false;
	} catch (...) {
		return true;
	}
}

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
	Rain::String::CommandLineParser parser;
	parser.addLayer("port", &port);
	parser.addLayer("forwarding-address", &forwardingAddress);
	parser.addLayer("password", &password);
	parser.addLayer("D", &domains);
	parser.addLayer("domain", &domains);
	parser.parse(argc - 1, argv + 1);

	domainsSet = std::set<std::string>(domains.begin(), domains.end());

	// Run server.
	Server server;
	server.serve(Rain::Networking::Host("*", port), false);
	std::cout << "Serving on port " << server.getService().getCStr() << ".\n";
	std::string command;
	while (command != "exit") {
		std::cin >> command;
	}
	return 0;
}
