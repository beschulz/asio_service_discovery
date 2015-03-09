#define BOOST_TEST_MODULE basic_tests
#include <boost/test/included/unit_test.hpp>

#include <betabugs/networking/service_announcer.hpp>
#include <betabugs/networking/service_discoverer.hpp>
#include <thread>

using namespace betabugs::networking;

BOOST_AUTO_TEST_SUITE( basic_tests )

    BOOST_AUTO_TEST_CASE( test_basic_functionality )
    {
        boost::asio::io_service io_service;


        service_announcer announcer(io_service, "my_service", 1337);

        bool did_discover_service = false;
        service_discoverer discoverer(io_service, "my_service",
        [&io_service, &did_discover_service]
        (const service_discoverer::services& services)
        {
            BOOST_CHECK(!services.empty());

            for(const auto& service : services)
            {
                BOOST_CHECK_EQUAL(service.service_name, "my_service");
                BOOST_CHECK_EQUAL(service.computer_name, boost::asio::ip::host_name());
                BOOST_CHECK_EQUAL(service.endpoint.port(), 1337);
            }

            did_discover_service = !services.empty();
        });

        boost::asio::deadline_timer timer(io_service);
        timer.expires_from_now(boost::posix_time::seconds(2));

        timer.async_wait(
        [&io_service, &did_discover_service]
        (const boost::system::error_code& error)
        {
            BOOST_CHECK(did_discover_service);
            io_service.stop();
        });

        io_service.run();
    }

    /*
    * Test, that only services subscribed to are passed to the callback
    * */
    BOOST_AUTO_TEST_CASE( test_service_filtering )
    {
        boost::asio::io_service io_service;

        bool did_discover_service = false;

        service_announcer announcer(io_service, "my_service", 1337);
        service_announcer announcer2(io_service, "my_service2", 1338);

        service_discoverer discoverer(io_service, "my_service",
                [&io_service, &did_discover_service]
                        (const service_discoverer::services& services)
                {
                    BOOST_CHECK(!services.empty());

                    for(const auto& service : services)
                    {
                        BOOST_CHECK_EQUAL(service.service_name, "my_service");
                        BOOST_CHECK_EQUAL(service.computer_name, boost::asio::ip::host_name());
                        BOOST_CHECK_EQUAL(service.endpoint.port(), 1337);
                    }

                    did_discover_service = !services.empty();
                });

        boost::asio::deadline_timer timer(io_service);
        timer.expires_from_now(boost::posix_time::seconds(2));

        timer.async_wait(
                [&io_service, &did_discover_service]
                (const boost::system::error_code& error)
                {
                    BOOST_CHECK(did_discover_service);
                    io_service.stop();
                });

        io_service.run();
    }

    BOOST_AUTO_TEST_CASE( test_threaded )
    {
        std::atomic<bool> did_discover_service;

        std::thread announcer_thread([&did_discover_service](){
            boost::asio::io_service io_service;

            service_announcer announcer(io_service, "my_service", 1337);

            boost::asio::deadline_timer timer(io_service);
            timer.expires_from_now(boost::posix_time::seconds(2));

            timer.async_wait(
            [&io_service, &did_discover_service]
                    (const boost::system::error_code& error)
            {
                BOOST_CHECK(did_discover_service);
                io_service.stop();
            });

            io_service.run();
        });

        std::thread discoverer_thread([&did_discover_service](){
            boost::asio::io_service io_service;

            service_discoverer discoverer(io_service, "my_service",
                [&io_service, &did_discover_service]
                (const service_discoverer::services& services)
                {
                    BOOST_CHECK(!services.empty());

                    for(const auto& service : services)
                    {
                        BOOST_CHECK_EQUAL(service.service_name, "my_service");
                        BOOST_CHECK_EQUAL(service.computer_name, boost::asio::ip::host_name());
                        BOOST_CHECK_EQUAL(service.endpoint.port(), 1337);
                    }

                    did_discover_service = !services.empty();

                    io_service.stop();
                });

            boost::asio::deadline_timer timer(io_service);
            timer.expires_from_now(boost::posix_time::seconds(2));

            timer.async_wait(
            [&io_service, &did_discover_service]
                    (const boost::system::error_code& error)
            {
                BOOST_CHECK(did_discover_service);
                io_service.stop();
            });

            io_service.run();
        });

        announcer_thread.join();
        discoverer_thread.join();

        BOOST_CHECK(did_discover_service);
    }

    BOOST_AUTO_TEST_CASE( test_overflow )
    {
        boost::asio::io_service io_service;

        std::string ridiculously_long_service_name;
        const char hex_chars[] = "0123456789abcdef";

        for(int i=0; i!=1024*8; ++i)
        {
            ridiculously_long_service_name.push_back(hex_chars[i % sizeof(hex_chars)]);
        }

        service_announcer announcer(io_service, ridiculously_long_service_name, 1337);

        bool did_discover_service = false;
        service_discoverer discoverer(io_service, ridiculously_long_service_name,
                [&io_service, &did_discover_service, &ridiculously_long_service_name]
                        (const service_discoverer::services& services)
                {
                    BOOST_CHECK(!services.empty());

                    for(const auto& service : services)
                    {
                        BOOST_CHECK_EQUAL(service.service_name, ridiculously_long_service_name);
                        BOOST_CHECK_EQUAL(service.computer_name, boost::asio::ip::host_name());
                        BOOST_CHECK_EQUAL(service.endpoint.port(), 1337);
                    }

                    did_discover_service = !services.empty();
                });

        boost::asio::deadline_timer timer(io_service);
        timer.expires_from_now(boost::posix_time::seconds(2));

        timer.async_wait(
                [&io_service, &did_discover_service]
                        (const boost::system::error_code& error)
                {
                    BOOST_CHECK(did_discover_service);
                    io_service.stop();
                });

        io_service.run();
    }

BOOST_AUTO_TEST_SUITE_END()
