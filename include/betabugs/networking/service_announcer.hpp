//
// service_announcer.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2015 Benjamin Schulz (beschulz at betabugs dot de)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BB_SERVICE_ANNOUNCER_HPP_INCLUDED
#define BB_SERVICE_ANNOUNCER_HPP_INCLUDED

#pragma once

#include <iostream>
#include <sstream>
#include <boost/asio.hpp>

namespace betabugs {
namespace networking {

/*!
* class to announce a named network service, that can be discovered via the service_discoverer.
*
* example:
* @code
* boost::asio::io_service io_service;
*
* service_announcer announcer(io_service, "my_awesome_service", 1337);
*
* io_service.run();
* @endcode
*
* Note: In case of error, this class just prints to std::cerr
*       In case of an unknown service, this class prints it to std::clog
* */
class service_announcer
{
  public:
	/*!
	* announce service named service_name listening on service_port in one second intervals.
	* Note, that it is not required, that the service actually listens on service_port and that
	* there is no coupling between the announcer and you service.
	* */
	service_announcer(
		boost::asio::io_service& io_service, ///< io_service to use
		const std::string& service_name, ///< the name of the announced service
		const unsigned short service_port, ///< the port where the service listens on
		const unsigned short multicast_port = 30001, ///< the port this udp multicast sender sends to
		const boost::asio::ip::address& multicast_address = boost::asio::ip::address::from_string("239.255.0.1") ///< mulicast address to use. see: http://en.wikipedia.org/wiki/Multicast_address
	)
		: endpoint_(multicast_address, multicast_port)
		, socket_(io_service, endpoint_.protocol())
		, timer_(io_service)
		, service_name_(service_name)
		, service_port_(service_port)
	{
		// TODO: implement via multiple sockets:
		// http://atastypixel.com/blog/the-making-of-talkie-multi-interface-broadcasting-and-multicast/
		write_message();
	}

  private:
	void handle_send_to(const boost::system::error_code& error)
	{
		if (error)
		{
			std::cerr << error.message() << std::endl;
		}
		else
		{
			timer_.expires_from_now(boost::posix_time::seconds(1));
			timer_.async_wait(
				[this](const boost::system::error_code& error)
				{
					this->handle_timeout(error);
				});
		}
	}

	void handle_timeout(const boost::system::error_code& error)
	{
		if (!error)
		{
			write_message();
		}
		else
		{
			std::cerr << error.message() << std::endl;
		}
	}

	void write_message()
	{
		std::ostringstream os;
		boost::system::error_code error_code;

		// "my_service_name:my_computer:2052"
		os << service_name_
			<< ":" << boost::asio::ip::host_name(error_code)
			<< ":" << service_port_;

		if (error_code)
		{
			std::cerr << error_code.message() << std::endl;
		}

		message_ = os.str();

		socket_.async_send_to(
			boost::asio::buffer(message_), endpoint_,
			[this](const boost::system::error_code& error, std::size_t /*bytes_transferred*/)
			{
				this->handle_send_to(error);
			}
		);
	}

  private:
	boost::asio::ip::udp::endpoint endpoint_;
	boost::asio::ip::udp::socket socket_;
	boost::asio::deadline_timer timer_;
	std::string message_;
	const std::string service_name_;
	const unsigned short service_port_;
};

}
}

#endif /* BB_SERVICE_ANNOUNCER_HPP_INCLUDED */
