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
#include <sstream>
#include <iomanip>
#include <blepp/blestatemachine.h>
#include <sys/time.h>
#include <unistd.h>
using namespace std;
using namespace BLEPP;

////////////////////////////////////////////////////////////////////////////////
//
// This program demonstrates the use of the library
// 
int main(int argc, char **argv)
{
	if(argc != 2)
	{	
		cerr << "Please supply address.\n";
		cerr << "Usage:\n";
		cerr << "prog address\n";
		exit(1);
	}

	BLEGATTStateMachine gatt;

	std::function<void()> cb = [&gatt](){

		//Note this won't work if you don't have a devices with these services and characteristics.
		//And you almost certainly don't.
		//substitute your own numbers here.
		for(auto& service: gatt.primary_services)
			if(service.uuid == UUID("7309203e-349d-4c11-ac6b-baedd1819764"))
				for(auto& characteristic: service.characteristics)
					if(characteristic.uuid == UUID("b8637601-a003-436d-a995-2a7f20bcb3d4"))
					{
						//Send a 1 (you can also send longer chunks of data too)
						characteristic.write_request(uint8_t(1));
					}
	};
	
	gatt.cb_disconnected = [](BLEGATTStateMachine::Disconnect d)
	{
		cerr << "Disconnect for reason " << BLEGATTStateMachine::get_disconnect_string(d) << endl;
		exit(1);
	};
	
	gatt.setup_standard_scan(cb);

	
	
	// finished has been called. These errors mean the program is incorrect.
	try
	{ 
		//This is how to use the blocking interface. It is very simple.
		gatt.connect_blocking(argv[1]);
		for(;;)
		{
			gatt.read_and_process_next();
		}
	}
	catch(std::runtime_error e)
	{
		cerr << "Something's stopping bluetooth working: " << e.what() << endl;
	}
	catch(std::logic_error e)
	{
		cerr << "Oops, someone fouled up: " << e.what() << endl;
	}

}
