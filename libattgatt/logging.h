/*
   Copyright (c) 2013  Edward Rosten

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
		* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of the <organization> nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#define LOG(X, Y) \
do{\
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
