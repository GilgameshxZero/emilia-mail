#include "emilia-mail.hpp"

int main() {
	std::cout << "Hello world! This is the emilia-mail." << std::endl;
	std::cout << "Using rain v"
						<< RAIN_VERSION_MAJOR << "."
						<< RAIN_VERSION_MINOR << "."
						<< RAIN_VERSION_REVISION << "."
						<< RAIN_VERSION_BUILD << "." << std::endl;
	std::cout << "This binary was built on "
						<< Rain::Platform::getPlatformString() << "." << std::endl;
	return 0;
}
