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

	//This class does all of the GATT interactions for you.
	//It's a callback based interface, so you need to provide 
	//callbacks to make it do anything useful. Don't worry: this is
	//a library, not a "framework", so it won't steal your main loop.
	//In other examples, I show you how to get and use the file descriptor, so 
	//you will only make calls into BLEGATTStateMachine when there's data
	//to process.
	BLEGATTStateMachine gatt;

	//This function will be called when a push notification arrives from the device.
	//Not much sanity/error checking here, just for clarity.
	//Basically, extract the float and log it along with the time.
	std::function<void(const PDUNotificationOrIndication&)> notify_cb = [&](const PDUNotificationOrIndication& n)
	{
		auto ms_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
		float temp = bluetooth_float_to_IEEE754(n.value().first+1);

		cout << setprecision(15) << ms_since_epoch.count()/1000. << " " << setprecision(5) << temp << endl;
	};
	
	//This is called when a complete scan of the device is done, giving
	//all services and characteristics. This one simply searches for the 
	//standardised "temperature" characteristic (aggressively cheating and not
	//bothering to check if the service is correct) and sets up the device to 
	//send us notifications.
	//
	//This will simply sit there happily connected in blissful ignorance if there's
	//no temperature characteristic.
	std::function<void()> found_services_and_characteristics_cb = [&gatt, &notify_cb](){
		for(auto& service: gatt.primary_services)
			for(auto& characteristic: service.characteristics)
				if(characteristic.uuid == UUID("2a1c"))
				{
					characteristic.cb_notify_or_indicate = notify_cb;
					characteristic.set_notify_and_indicate(true, false);
				}
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
		cerr << "Disconnect for reason " << BLEGATTStateMachine::get_disconnect_string(d) << endl;
		exit(1);
	};
	
	//This is how to use the blocking interface. It is very simple. You provide the main 
	//loop and just hammer on the state machine struct. 
	gatt.connect_blocking(argv[1]);
	for(;;)
		gatt.read_and_process_next();

}
