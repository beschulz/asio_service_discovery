//
// service_discoverer.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2015 Benjamin Schulz (beschulz at betabugs dot de)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Created by Benjamin Schulz on 14/03/15.
//

#ifndef _ASIO_SERVICE_DISCOVERY_STD_CHRONO_TIME_TRAITS_HPP_
#define _ASIO_SERVICE_DISCOVERY_STD_CHRONO_TIME_TRAITS_HPP_

#pragma once

#include <chrono>

namespace betabugs {
namespace networking {
namespace detail {

/*!
* Time trait class to be used with boost::asio::deadline_timer.
* Enabled usage of std::crono with boost::asio::deadline_timer
*
* typedef boost::asio::basic_deadline_timer<
*        std::system_clock,
*        std_chrono_time_traits<std::system_clock>> my_system_clock_deadline_timer
*
* http://stackoverflow.com/questions/16721243/boostasiodeadline-timer-with-stdchrono-time-values
*
* */
template<typename Clock>
struct std_chrono_time_traits
{
	typedef typename Clock::time_point time_type;
	typedef typename Clock::duration duration_type;

	static time_type now()
	{
		return Clock::now();
	}

	static time_type add(time_type t, duration_type d)
	{
		return t + d;
	}

	static duration_type subtract(time_type t1, time_type t2)
	{
		return t1 - t2;
	}

	static bool less_than(time_type t1, time_type t2)
	{
		return t1 < t2;
	}

	static boost::posix_time::time_duration
	to_posix_duration(duration_type d1)
	{
		using std::chrono::duration_cast;
		auto in_sec = duration_cast<std::chrono::seconds>(d1);
		auto in_usec = duration_cast<std::chrono::microseconds>(d1 - in_sec);
		boost::posix_time::time_duration result =
			boost::posix_time::seconds(in_sec.count()) +
				boost::posix_time::microseconds(in_usec.count());
		return result;
	}
};

}
}
}

#endif //_ASIO_SERVICE_DISCOVERY_STD_CHRONO_TIME_TRAITS_HPP_
