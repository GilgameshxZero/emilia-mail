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
	std::vector<std::string> domains = {"gilgamesh.cc", "emilia.best"};
	Rain::String::CommandLineParser parser;
	parser.addLayer("port", &port);
	parser.addLayer("forwarding-address", &forwardingAddress);
	parser.addLayer("password", &password);
	parser.addLayer("D", &domains);
	parser.addLayer("domain", &domains);
	parser.parse(argc - 1, argv + 1);

	return 0;
}
