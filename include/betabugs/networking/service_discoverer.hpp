//
// service_discoverer.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2015 Benjamin Schulz (beschulz at betabugs dot de)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <string>
#include <set>
#include <boost/asio.hpp>

namespace betabugs {
namespace networking {

/*!
* Class to discover services announced by the service_announcer
* use service_discoverer discoverer(io_service, name_of_my_service, on_service_discovered);
* on_service_discovered is a std::function, that gets a services set passed as its first and only
* argument.
*
* Note: In case of error, this class just prints to std::cerr
*
* */
class service_discoverer
{
  public:
    struct service
    {
      /* const */ std::string service_name; ///< the name of the service
      /* const */ std::string computer_name; ///< the name of the computer the service is running on
      /* const */ boost::asio::ip::tcp::endpoint endpoint; ///< enpoint you should connect to
      /* const */ std::chrono::steady_clock::time_point last_seen;

      bool operator < (const service& o) const
      {
        // so that we can stick service objects into sets. last_seen is ignored
          if(service_name != o.service_name) return std::less<std::string>()(service_name, o.service_name);
          if(computer_name != o.computer_name) return std::less<std::string>()(computer_name, o.computer_name);
          if(endpoint != o.endpoint) return std::less<boost::asio::ip::tcp::endpoint>()(endpoint, o.endpoint);
          return false; // they are equal
      }

      bool operator == (const service& o) const
      {
        // again, last_seen is ignored
        return
          std::equal_to<std::string>()(service_name, o.service_name) &&
          std::equal_to<std::string>()(computer_name, o.computer_name) &&
          std::equal_to<boost::asio::ip::tcp::endpoint>()(endpoint, o.endpoint)
        ;
      }

      double age_in_seconds() const
      {
        auto age = std::chrono::steady_clock::now() - last_seen;
        return std::chrono::duration_cast<std::chrono::duration<double>>(age).count();
      }

      // this uses "name injection"
      friend std::ostream& operator << (std::ostream& os, const service_discoverer::service& service)
      {
        os << service.service_name << " on " << service.computer_name << "(" << service.endpoint << ") " <<
          service.age_in_seconds() << " seconds ago";
        return os;
      }
    };

    typedef std::set<service> services;
    services discovered_services;

    typedef std::function<void (const services & services)> on_service_discovered_t;

    service_discoverer(boost::asio::io_service& io_service,
      const std::string& listen_for_service, // the service to watch out for
      const on_service_discovered_t on_service_discovered = on_service_discovered_t(),
      const short multicast_port = 30001,
      const boost::asio::ip::address& listen_address=boost::asio::ip::address::from_string("0.0.0.0"),
      const boost::asio::ip::address& multicast_address=boost::asio::ip::address::from_string("239.255.0.1")
    )
     : listen_for_service_(listen_for_service)
     , socket_(io_service)
     , on_service_discovered_(on_service_discovered)
    {
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
      { // simpleton parser
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
      catch(const std::exception& e)
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

      if(service_name == listen_for_service_)
      {
        // we need to do a replace here, because discovered_service might compare equal
        // to an item already in the set. In this case no asignment would be performed and
        // therefore last_seen would not be updated
        discovered_services.erase(discovered_service);
        discovered_services.insert(discovered_service);

        if(on_service_discovered_)
        {
          on_service_discovered_(discovered_services);
        }
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
                        this->handle_message({ receive_buffer->data(), receive_buffer->data() + bytes_recvd }, *sender_endpoint);
                        start_receive();
                    }
                });

        });
    }

    const std::string listen_for_service_;
    boost::asio::ip::udp::socket socket_;
    on_service_discovered_t on_service_discovered_;
};

}
}
