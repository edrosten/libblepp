/*
 *
 *  libattgatt - Implementation of the Generic ATTribute Protocol
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
#include <libattgatt/xtoa.h>

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
	"warning",
	"info",
	"debug",
	"trace"
};

extern LogLevels log_level;

#define LOG(X, Y) do{\
	if(X <= log_level)\
	{\
		if(X >= LogLevels::Debug)\
			std::clog << log_types[X] << ": " << __FUNCTION__ << " " << Y << std::endl;\
		else\
			std::clog << log_types[X] << ": " << Y << std::endl;\
	}\
}while(0)

struct EnterThenLeave
{
	std::string who;
	EnterThenLeave(const std::string& s)
	:who(s)
	{
		if(log_level >= Trace)\
			std::clog << log_types[Trace] << ": " << who << " entering\n";
	}

	~EnterThenLeave()
	{
		if(log_level >= Trace)\
			std::clog << log_types[Trace] << ": " << who << " leaving\n";
	}

};

#define ENTER() EnterThenLeave log_enter_then_leave(__FUNCTION__);

#endif
