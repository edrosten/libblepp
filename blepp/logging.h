/*
 *
 *  blepp - Implementation of the Generic ATTribute Protocol
 *
 *  Copyright (C) 2013, 2014 Edward Rosten
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __INC_LIBATTGATT_LOGGING_H
#define __INC_LIBATTGATT_LOGGING_H
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <blepp/xtoa.h>

namespace BLEPP
{

	enum LogLevels
	{
		Error,
		Warning,
		Info,
		Debug,
		Trace
	};

	static const char* log_types[] = 
	{
		"error",
		"warn ",
		"info ",
		"debug",
		"trace"
	};
}
namespace BLEPP{

	extern LogLevels log_level;

	template<class T> const T& log_no_uint8(const T& a)
	{
		return a;
	}
	inline int log_no_uint8(uint8_t a)
	{
		return a;
	}
	
	inline std::ostream& log_line_header(LogLevels x, const char* function, const int line, const char* file)
	{
		std::clog << log_types[x] << " " <<  std::fixed << std::setprecision(6) << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::system_clock::now().time_since_epoch()).count();
		if(log_level >= Debug)
			std::clog << " " << function;
		if(log_level >= Trace)
			std::clog << " " << file << ":" << line;
		std::clog << ": ";
		return std::clog;
	}


	#define LOGVAR(Y, X) LOG(Y,  #X << " = " << log_no_uint8(X))
	#define LOGVARHEX(Y, X) LOG(Y,  #X << " = " << std::hex <<log_no_uint8(X) <<std::dec)
	#define LOG(X, Y) do{\
		if(X <= BLEPP::log_level)\
			log_line_header(X, __FUNCTION__, __LINE__, __FILE__) << Y << std::endl;\
	}while(0)

	struct EnterThenLeave
	{
		const char* who;
		int where;
		const char* file;
		EnterThenLeave(const char *s, int w, const char* f)
		:who(s),where(w), file(f)
		{
			if(BLEPP::log_level >= Trace)
				log_line_header(Trace, who, where, file) << "entering" << std::endl;
		}

		~EnterThenLeave()
		{
			if(BLEPP::log_level >= Trace)
				log_line_header(Trace, who, where, file) << "leaving" << std::endl;
		}

	};

	#define ENTER() EnterThenLeave log_enter_then_leave(__FUNCTION__, __LINE__, __FILE__);
}
#endif
