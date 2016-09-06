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
#include <iomanip>
#include <blepp/blestatemachine.h>
#include <unistd.h>
#include <chrono>
using namespace std;
using namespace chrono;
using namespace BLEPP;

////////////////////////////////////////////////////////////////////////////////
//
// This program demonstrates the use of the library
// 
int main(int argc, char **argv)
{
	if(argc != 2 && argc != 3)
	{	
		cerr << "Please supply address.\n";
		cerr << "Usage:\n";
		cerr << "prog address [nonblocking]\n";
		exit(1);
	}

	log_level = Info;

	
	BLEGATTStateMachine gatt;


	std::function<void(const PDUNotificationOrIndication&)> notify_cb = [&](const PDUNotificationOrIndication& n)
	{
		auto ms_since_epoch = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
		const uint8_t* d = n.value().first;
		int emg = ((0+d[1] *256 + d[0])>>0) ;

		int volt = ((0+d[7] *256 + d[6])>>0) ;
		double v=-1;
		
		if(volt != 0x8000)
			v = volt / 1000.0;
		
		cout << setprecision(15) << ms_since_epoch.count()/1000. << " " << emg << " " << v << endl;
	};
	

	std::function<void()> cb = [&gatt, &notify_cb](){
		pretty_print_tree(gatt);

		for(auto& service: gatt.primary_services)
			for(auto& characteristic: service.characteristics)
				if(characteristic.uuid == UUID("53f72b8c-ff27-4177-9eee-30ace844f8f2"))
				{
					characteristic.cb_notify_or_indicate = notify_cb;
					characteristic.set_notify_and_indicate(true, false);
				}
	};
	
	gatt.cb_disconnected = [](BLEGATTStateMachine::Disconnect d)
	{
		cerr << "Disconnect for reason " << BLEGATTStateMachine::get_disconnect_string(d) << endl;
		exit(1);
	};
	

	gatt.setup_standard_scan(cb);


	//This is how to use the blocking interface. It is very simple.
	gatt.connect_blocking(argv[1]);
	for(;;)
		gatt.read_and_process_next();

}
