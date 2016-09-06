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
#include <blepp/pretty_printers.h>

#include <sstream>
#include <iomanip>
using namespace std;

namespace BLEPP
{

	std::string to_hex(const std::uint16_t& u)
	{
		stringstream os;
		os << setw(4) << setfill('0') << hex << u;
		return os.str();
	}

	std::string to_hex(const std::uint8_t& u)
	{
		stringstream os;
		os << setw(2) << setfill('0') << hex << (int)u;
		return os.str();
	}

	std::string to_str(const std::uint8_t& u)
	{
		if(u < 32 || u > 126)
			return "\\x" + to_hex(u);
		else
		{
			char buf[] = {(char)u, 0};
			return buf;
		}
	}

	std::string to_str(const bt_uuid_t& uuid)
	{
		//8 4 4 4 12
		if(uuid.type == BT_UUID16)
			return to_hex(uuid.value.u16);
		else if(uuid.type == BT_UUID128)
		{
			char s[] = "xoxoxoxo-xoxo-xoxo-xoxo-xoxoxoxoxoxo";
			bt_uuid_to_string(&uuid, s, sizeof(s));
			return s;
		}
		else
			return "uuid.wtf";

	}

	std::string to_hex(const std::uint8_t* d, int l)
	{
		stringstream os;
		for(int i=0; i < l; i++)
			os << to_hex(d[i]) << " ";
		return os.str();
	}
	std::string to_hex(pair<const std::uint8_t*, int> d)
	{
		return to_hex(d.first, d.second);
	}

	std::string to_hex(const vector<std::uint8_t>& v)
	{
		return to_hex(v.data(), v.size());
	}

	std::string to_str(const std::uint8_t* d, int l)
	{
		stringstream os;
		for(int i=0; i < l; i++)
			os << to_str(d[i]);
		return os.str();
	}
	std::string to_str(pair<const std::uint8_t*, int> d)
	{
		return to_str(d.first, d.second);
	}
	std::string to_str(pair<const std::uint8_t*, const std::uint8_t*> d)
	{
		return to_str(d.first, d.second - d.first);
	}

	std::string to_str(const vector<std::uint8_t>& v)
	{
		return to_str(v.data(), v.size());
	}

}
