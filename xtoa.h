#ifndef INC_XTOA_H
#define INC_XTOA_H
#include <sstream>
template<class X>
std::string xtoa(const X& x)
{
	std::ostringstream o;
	o << x;
	return o.str();
}
#endif
