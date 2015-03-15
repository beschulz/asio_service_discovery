//
// service_discoverer.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2015 Benjamin Schulz (beschulz at betabugs dot de)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BB_SERVICE_DISCOVERER_HPP_INCLUDED
#define BB_SERVICE_DISCOVERER_HPP_INCLUDED

#pragma once

#include <iostream>
#include <string>
#include <set>
#include <boost/asio.hpp>
#include "detail/std_chrono_time_traits.hpp"

namespace betabugs {
namespace networking {

/*!
* Class to discover services announced by the service_announcer
* use service_discoverer discoverer(io_service, name_of_my_service, on_service_discovered);
* on_service_discovered is a std::function, that gets a services set passed as its first and only
* argument.
*
* example:
* @code
*
* boost::asio::io_service io_service;
*
* service_discoverer discoverer(
* 	io_service,
* 	"my_awesome_service",
* 	[](const service_discoverer::services& services)
* 	{
* 		for(auto& service : services)
* 		{
* 			std::cout << "discovered: " service << std::endl;
* 		}
* 	}
* );
*
* io_service.run();
* @endcode
*
* Note: In case of error, this class just prints to std::cerr
*
* */
class service_discoverer
{
  public:
	/*!
	* Represents a discovered service
	* */
	struct service
	{
		/* const */ std::string service_name; ///< the name of the service
		/* const */ std::string computer_name; ///< the name of the computer the service is running on
		/* const */ boost::asio::ip::tcp::endpoint endpoint; ///< enpoint you should connect to. Even though, it's an tcp endpoint, it's up to you, what you do with the data.
		/* const */ std::chrono::steady_clock::time_point last_seen;

		bool operator<(const service& o) const
		{
			// last_seen is ignored
			return std::tie(service_name, computer_name, endpoint) <
				std::tie(o.service_name, o.computer_name, o.endpoint);
		}

		bool operator==(const service& o) const
		{
			// again, last_seen is ignored
			return std::tie(service_name, computer_name, endpoint) ==
				std::tie(o.service_name, o.computer_name, o.endpoint);
		}

		double age_in_seconds() const
		{
			auto age = std::chrono::steady_clock::now() - last_seen;
			return std::chrono::duration_cast<std::chrono::duration<double>>(age).count();
		}

		// this uses "name injection"
		friend std::ostream& operator<<(std::ostream& os, const service_discoverer::service& service)
		{
			os << service.service_name << " on " << service.computer_name << "(" << service.endpoint << ") " <<
				service.age_in_seconds() << " seconds ago";
			return os;
		}
	};

	/// a set of discovered services
	typedef std::set<service> services;

	/// this callback gets called, when ever the set of available services changes
	typedef std::function<void(const services& services)> on_services_changed_t;

	/*!
	* Constructs a service_discoverer.
	* ================================
	*
	* listen for udp multicast packets announcing listen_for_service on listen_address:multicast_port.
	*
	* Call on_services_changed each time a service has been discovered or a service has been idle for too long.
	* To protect against malicious announcers, there is a limit (max_services) on how many services will end up in the set of
	* discovered services.
	* */
	service_discoverer(boost::asio::io_service& io_service, ///< io_service to use
		const std::string& listen_for_service, ///< the service to watch out for
		const on_services_changed_t on_services_changed, ///< gets called when ever the set of discovered services changes
		const std::chrono::steady_clock::duration max_idle = std::chrono::seconds(30), ///< services not seen for this amount of time will be removed from the set
		const size_t max_services = 10, ///< maximum number of services to hold
		const unsigned short multicast_port = 30001, ///< the udp multicast port to listen on
		const boost::asio::ip::address& listen_address = boost::asio::ip::address::from_string("0.0.0.0"), ///< address to listen on
		const boost::asio::ip::address& multicast_address = boost::asio::ip::address::from_string("239.255.0.1") ///< must match the one used in service_announcer
	)
		: listen_for_service_(listen_for_service)
		, socket_(io_service)
		, idle_check_timer_(io_service)
		, on_services_changed_(on_services_changed)
		, max_idle_(max_idle)
		, max_services_(max_services)
	{
		assert(max_services_ > 0);

		// Create the socket so that multiple may be bound to the same address.
		boost::asio::ip::udp::endpoint listen_endpoint(
			listen_address, multicast_port);
		socket_.open(listen_endpoint.protocol());
		socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
		socket_.bind(listen_endpoint);

		// Join the multicast group.
		socket_.set_option(
			boost::asio::ip::multicast::join_group(multicast_address));

		start_receive();
	}

  private:
	void handle_message(const std::string& message, const boost::asio::ip::udp::endpoint& sender_endpoint)
	{
		std::vector<std::string> tokens;
		{ // simpleton "parser"
			std::istringstream f(message);
			std::string s;
			while (getline(f, s, ':'))
				tokens.push_back(s);

			if (tokens.size() != 3)
			{
				std::cerr << "invalid number of tokens in received service announcement: " << std::endl;
				std::cerr << "  message: " << message << std::endl;
				std::cerr << "  tokens: " << tokens.size() << std::endl;
				return;
			}
		}
		assert(tokens.size() == 3);

		std::string service_name = tokens[0];
		std::string computer_name = tokens[1];
		std::string port_string = tokens[2];

		// unsigned long, because it's the smalles value that suports unsigned parsing via stl :/
		unsigned long port = 0;

		try
		{
			port = std::stoul(port_string);
		}
		catch (const std::exception& e)
		{
			std::cerr << "failed to parse port number from: " << port_string << std::endl;
			return;
		}

		if (port > std::numeric_limits<unsigned short>::max())
		{
			std::cerr << "failed to parse port number from: " << port_string << std::endl;
			return;
		}

		auto discovered_service = service
			{
				service_name,
				computer_name,
				boost::asio::ip::tcp::endpoint(sender_endpoint.address(), port),
				std::chrono::steady_clock::now()
			};

		if (service_name == listen_for_service_)
		{
			// we need to do a replace here, because discovered_service might compare equal
			// to an item already in the set. In this case no assignment would be performed and
			// therefore last_seen would not be updated
			discovered_services_.erase(discovered_service);
			discovered_services_.insert(discovered_service);

			remove_idle_services();

			// if we have to much services, we need to drop the oldest one
			if (discovered_services_.size() > max_services_)
			{
				// determine service whose last_seen time point is the smallest (i.e. the oldest)
				services::iterator oldest_pos =
					std::min_element(
						discovered_services_.begin(),
						discovered_services_.end(),
						[](const service& a, const service& b)
						{
							return a.last_seen < b.last_seen;
						}
					);
				assert(oldest_pos != discovered_services_.end())discovered_services_;
				discovered_services_.erase(oldest_pos);
			}

			{ // manage the idle_check_timer in case the service dies and we receive no other announcements

				{ // cancel the idle_check_timer
					boost::system::error_code ec;
					idle_check_timer_.cancel(ec);
					if (ec)
						std::cerr << ec.message();
				}

				{ // determine new point in time for the timer
					services::iterator oldest_pos =
						std::min_element(
							discovered_services_.begin(),
							discovered_services_.end(),
							[](const service& a, const service& b)
							{
								return a.last_seen < b.last_seen;
							}
						);
					assert(oldest_pos != discovered_services_.end())discovered_services_;

					idle_check_timer_.expires_at(oldest_pos->last_seen + max_idle_);
					idle_check_timer_.async_wait(
						[this](const boost::system::error_code& ec)
						{
							if (!ec && remove_idle_services())
							{
								on_services_changed_(discovered_services_);
							}
						}
					);
				}
			}

			on_services_changed_(discovered_services_);
		}
		else
		{
			std::clog << "ignoring: " << discovered_service << std::endl;
		}
	}

	void start_receive()
	{
		// first do a receive with null_buffers to determine the size
		socket_.async_receive(boost::asio::null_buffers(),
			[this](const boost::system::error_code& error, unsigned int)
			{
				if (error)
				{
					std::cerr << error.message() << std::endl;
				}
				else
				{
					size_t bytes_available = socket_.available();

					auto receive_buffer = std::make_shared<std::vector<char>>(bytes_available);
					auto sender_endpoint = std::make_shared<boost::asio::ip::udp::endpoint>();

					socket_.async_receive_from(
						boost::asio::buffer(receive_buffer->data(), receive_buffer->size()), *sender_endpoint,
						[this, receive_buffer, sender_endpoint] // we hold on to the shared_ptrs, so that it does not delete it's contents
							(const boost::system::error_code& error, size_t bytes_recvd)
						{
							if (error)
							{
								std::cerr << error.message() << std::endl;
							}
							else
							{
								this->handle_message({receive_buffer->data(), receive_buffer->data() + bytes_recvd}, *sender_endpoint);
								start_receive();
							}
						});
				}
			});
	}


	// throw out services that have not been seen for to long, returns true, if at least one service was removed, false otherwise.
	bool remove_idle_services()
	{
		auto dead_line = std::chrono::steady_clock::now() - max_idle_;
		bool services_removed = false;

		for (services::const_iterator i = discovered_services_.begin(); i != discovered_services_.end();)
		{
			if (i->last_seen < dead_line)
			{
				i = discovered_services_.erase(i);
				services_removed = true;
			}
			else
				++i;
		}

		return services_removed;
	}

	typedef boost::asio::basic_deadline_timer<
		std::chrono::steady_clock,
		detail::std_chrono_time_traits<std::chrono::steady_clock>> steady_clock_deadline_timer_t;

	const std::string listen_for_service_;
	boost::asio::ip::udp::socket socket_;
	steady_clock_deadline_timer_t idle_check_timer_;
	const on_services_changed_t on_services_changed_;
	const std::chrono::steady_clock::duration max_idle_;
	const size_t max_services_;

	services discovered_services_;
};

}
}

#endif /* BB_SERVICE_DISCOVERER_HPP_INCLUDED */
