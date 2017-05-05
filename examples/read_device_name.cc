/*
 *
 *  libble++ - Implementation of the Generic ATTribute Protocol
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
#include <iomanip>
#include <blepp/blestatemachine.h>
#include <blepp/float.h>  //BLE uses the obscure IEEE11073 decimal exponent floating point values
#include <unistd.h>
#include <chrono>
using namespace std;
using namespace chrono;
using namespace BLEPP;

int main(int argc, char **argv)
{
	if(argc != 2)
	{	
		cerr << "Please supply address.\n";
		cerr << "Usage:\n";
		cerr << "prog <addres>";
		exit(1);
	}

	log_level = Error;

	BLEGATTStateMachine gatt;
	
	//This is called when a complete scan of the device is done, giving
	//all services and characteristics. This one simply searches for the 
	//standardised "temperature" characteristic (aggressively cheating and not
	//bothering to check if the service is correct) and sets up the device to 
	//send us notifications.
	//
	//This will simply sit there happily connected in blissful ignorance if there's
	//no temperature characteristic.
	std::function<void()> found_services_and_characteristics_cb = [&gatt](){
		for(auto& service: gatt.primary_services)
			for(auto& characteristic: service.characteristics)
				if(characteristic.uuid == UUID("2a00"))
				{
					characteristic.cb_read = [&](const PDUReadResponse& r)
					{
						cout << "Hello, my name is: ";
						auto v = r.value();
						cout << string(v.first, v.second) << ". You killed my father. repare to die." << endl;
						gatt.close();
					};
						
					characteristic.read_request();
					goto name_found;
				}

		cerr << "No device name found." << endl;
		gatt.close();
		name_found:
		;	
	};
	
	//This is the simplest way of using a bluetooth device. If you call this 
	//helper function, it will put everything in place to do a complete scan for
	//services and characteristics when you connect. If you want to save a small amount
	//of time on a connect and avoid the complete scan (you are allowed to cache this 
	//information in certain cases), then you can provide your own callbacks.
	gatt.setup_standard_scan(found_services_and_characteristics_cb);

	//I think this one is reasonably clear?
	gatt.cb_disconnected = [](BLEGATTStateMachine::Disconnect d)
	{
		if(d.reason != BLEGATTStateMachine::Disconnect::ConnectionClosed)
		{
			cerr << "Disconnect for reason " << BLEGATTStateMachine::get_disconnect_string(d) << endl;
			exit(1);
		}
		else
			exit(0);
	};
	
	//This is how to use the blocking interface. It is very simple. You provide the main 
	//loop and just hammer on the state machine struct. 
	gatt.connect_blocking(argv[1]);
	for(;;)
		gatt.read_and_process_next();

}
