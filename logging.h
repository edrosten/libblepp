#ifndef INCLUDE_LOGGING_H
#define INCLUDE_LOGGING_H
#include <iostream>
#include <sstream>
#include "xtoa.h"

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
