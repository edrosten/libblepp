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


#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <tuple>
#include <stdexcept>
#include <functional>
#include <algorithm>

#include  <libattgatt/logging.h>
#include  <libattgatt/bledevice.h>
#include <libattgatt/att_pdu.h>
#include <libattgatt/pretty_printers.h>
#include <libattgatt/blestatemachine.h>
using namespace std;

int main(int argc, char **argv)
{
	if(argc != 2)
	{	
		cerr << "Please supply address.\n";
		exit(1);
	}

	log_level = Warning;
	vector<uint8_t> buf(256);

	BLEGATTStateMachine gatt(argv[1]);

	std::function<void()> cb = [&gatt](){
		for(auto& service: gatt.primary_services)
			for(auto& characteristic: service.characteristics)
				if(service.uuid == UUID(0x1809) && characteristic.uuid == UUID(0x2a1c))
				{
					characteristic.cb_notify_or_indicate = [](const PDUNotificationOrIndication& n)
					{
						//cerr << "Hello: "  << ((n.value().first[0] + (n.value().first[1]<<8))>>4) << endl;
						cerr << "Hello: "  << hex  << setfill('0') << setw(4) << ((0+n.value().first[1] *256 + n.value().first[0])>>0) << dec << endl;

					};

					characteristic.set_notify_and_indicate(true, false);
				}
	};

	gatt.do_standard_scan(cb);

	try{
		for(;;)
		{
			gatt.read_and_process_next();
		}
	}
	catch(const char* err)
	{
		cerr << "Oh dear: " << err << endl;
	}
}
