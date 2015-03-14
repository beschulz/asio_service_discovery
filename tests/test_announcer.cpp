#include <betabugs/networking/service_announcer.hpp>

int main(int argc, const char* const argv[])
{
	(void) argc;
	(void) argv;

	boost::asio::io_service io_service;
	betabugs::networking::service_announcer announcer(io_service, "my_service", 1337);
	io_service.run();
	return 0;
}
