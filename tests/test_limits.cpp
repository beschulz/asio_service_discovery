#ifndef BOOST_TEST_MODULE
#	define BOOST_TEST_MODULE limit_tests
#	define BOOST_TEST_DYN_LINK
#	include <boost/test/unit_test.hpp>
#endif /* BOOST_TEST_MODULE */

#include <betabugs/networking/service_announcer.hpp>
#include <betabugs/networking/service_discoverer.hpp>
#include <thread>

using namespace betabugs::networking;

BOOST_AUTO_TEST_SUITE(basic_limits)

BOOST_AUTO_TEST_CASE(test_max_services)
{
	boost::asio::io_service io_service;

	using namespace betabugs::networking;

	unsigned short port = 1337;
	std::array<service_announcer, 5> announcers{{
		{io_service, "test_service", ++port},
		{io_service, "test_service", ++port},
		{io_service, "test_service", ++port},
		{io_service, "test_service", ++port},
		{io_service, "test_service", ++port},
	}};

	size_t max_found_services = 0;
	service_discoverer discoverer(
		io_service,
		"test_service",
		[&max_found_services](const service_discoverer::services& services)
		{
			BOOST_CHECK_LE(services.size(), 3);
			max_found_services = std::max(max_found_services, services.size());
		},
		std::chrono::seconds(5),
		3
	);

	boost::asio::deadline_timer timer(io_service);
	timer.expires_from_now(boost::posix_time::seconds(5));

	timer.async_wait(
		[&io_service](const boost::system::error_code& error)
		{
			BOOST_CHECK(!error);
			io_service.stop();
		}
	);

	io_service.run();
	BOOST_CHECK_EQUAL(max_found_services, 3);
}

BOOST_AUTO_TEST_CASE(test_max_idle)
{
	boost::asio::io_service io_service;

	using namespace betabugs::networking;

	unsigned short port = 1337;
	auto announcer_busy = std::make_shared<service_announcer>(io_service, "test_service", ++port);
	auto announcer_idle = std::make_shared<service_announcer>(io_service, "test_service", ++port);

	size_t max_found_services = 0;
	service_discoverer discoverer(
		io_service,
		"test_service",
		[&max_found_services, &announcer_idle](const service_discoverer::services& services)
		{
			for (const auto& service : services)
			{
				BOOST_CHECK_LE(service.age_in_seconds(), 2.0f);
			}

			// drop the service, so that it will be thrown out
			announcer_idle.reset();

			max_found_services = std::max(max_found_services, services.size());
		},
		std::chrono::seconds(2),
		1000
	);

	boost::asio::deadline_timer timer(io_service);
	timer.expires_from_now(boost::posix_time::seconds(5));

	timer.async_wait(
		[&io_service](const boost::system::error_code& error)
		{
			BOOST_CHECK(!error);
			io_service.stop();
		}
	);

	io_service.run();
	BOOST_CHECK_EQUAL(max_found_services, 2);
}

// test that the deadline timer fires and removes the idle service
BOOST_AUTO_TEST_CASE(test_max_idle_with_no_other_service)
{
	boost::asio::io_service io_service;

	using namespace betabugs::networking;

	unsigned short port = 1337;
	auto announcer_idle = std::make_shared<service_announcer>(io_service, "test_service", ++port);

	size_t min_found_services = std::numeric_limits<size_t>::max();
	size_t max_found_services = 0;

	service_discoverer discoverer(
		io_service,
		"test_service",
		[&min_found_services, &max_found_services, &announcer_idle](const service_discoverer::services& services)
		{
			for (const auto& service : services)
			{
				BOOST_CHECK_LE(service.age_in_seconds(), 2.0f);
			}

			// drop the service, so that it will be thrown out
			announcer_idle.reset();

			max_found_services = std::max(max_found_services, services.size());
			min_found_services = std::min(min_found_services, services.size());
		},
		std::chrono::seconds(2),
		1000
	);

	boost::asio::deadline_timer timer(io_service);
	timer.expires_from_now(boost::posix_time::seconds(4));

	timer.async_wait(
		[&io_service](const boost::system::error_code& error)
		{
			BOOST_CHECK(!error);
			io_service.stop();
		}
	);

	io_service.run();
	BOOST_CHECK_EQUAL(max_found_services, 1);
	BOOST_CHECK_EQUAL(min_found_services, 0);
}

BOOST_AUTO_TEST_SUITE_END()
