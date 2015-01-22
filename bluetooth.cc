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
#include <libattgatt/blestatemachine.h>
#include <libattgatt/float.h>
#include <deque>
#include <unistd.h>
#include "cxxgplot.h"  //lolzworthy plotting program
using namespace std;

void bin(uint8_t i)
{
	for(int b=7; b >= 0; b--)
		cout << !!(i & (1<<b));

}


int main(int argc, char **argv)
{
	cerr << "Hello, my name is Inigo Montoya\n";
	if(argc != 2)
	{	
		cerr << "Please supply address.\n";
		exit(1);
	}

	log_level = Trace;

	BLEGATTStateMachine gatt;
	
	cplot::Plotter plot;

	plot.range = " [ ] [0:1000] ";

	deque<int> points;


	//Function that reads an indication and formats it for plotting.
	std::function<void(const PDUNotificationOrIndication&)> notify_cb = [&](const PDUNotificationOrIndication& n)
	{
		//This particular device sends 16 bit integers.
		//Extract them and both print them in binary and send them to the plotting program
		const uint8_t* d = n.value().first;
		int val = ((0+d[1] *256 + d[0])>>0) ;

		cout << "Hello: "  << dec  << setfill('0') << setw(6) << val << dec << " ";
		bin(d[1]);
		cout << " ";
		bin(d[0]);
		cout << endl;

		points.push_back(val);
		if(points.size() > 100)
			points.pop_front();
		
		plot.newline("line lw 10 lt 0 title \"\"");
		plot.addpts(points);

		plot.draw();
	};


	//Search for the service and attribute and set up notifications and the appropriate callback.
	std::function<void()> cb = [&gatt, &notify_cb](){

		pretty_print_tree(gatt);

		for(auto& service: gatt.primary_services)
			for(auto& characteristic: service.characteristics)
				if(service.uuid == UUID("7309203e-349d-4c11-ac6b-baedd1819764") && characteristic.uuid == UUID("53f72b8c-ff27-4177-9eee-30ace844f8f2"))
				{
					characteristic.cb_notify_or_indicate = notify_cb;
					characteristic.set_notify_and_indicate(true, false);
				}
	};

	//Set up callback for disconnection
	gatt.cb_disconnected = [](BLEGATTStateMachine::Disconnect d)
	{
		cerr << "Disconnect for reason " << (int)d << endl;
		exit(1);
	};

	//Almost always we want to query the whole service/attribute tree
	//So, there is a function to do this automatically.
	gatt.setup_standard_scan(cb);

	
	//By default connect is a blocking call.
	gatt.connect(argv[1], false);


	
	fd_set write_set, read_set;

	for(;;)
	{
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);

		//Reads are always a possibility due to asynchronus notifications.
		FD_SET(gatt.socket(), &read_set);
		
		if(gatt.wait_on_write())
			FD_SET(gatt.socket(), &write_set);
			
		
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		int result = select(gatt.socket() + 1, &read_set, &write_set, NULL, & tv);

		if(FD_ISSET(gatt.socket(), &write_set))
		{
			cerr << "Write available\n";
			gatt.write_and_process_next();
		}

		if(FD_ISSET(gatt.socket(), &read_set))
			gatt.read_and_process_next();

		cerr << ".";

	}





/*	

	cout << "Hello\n";	
	char spin[]=R"(-\|/)";
	for(int i=0; ; i++)
	{

		fd_set wrset;
		FD_ZERO(&wrset);
		FD_SET(gatt.socket(), &wrset);
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 50000;
		int result = select(gatt.socket() + 1, NULL, &wrset, NULL, & tv);
		
		if(result == 0)
			cout << spin[i%4] << "\b" << flush;
		else
		{
			cout << " \b" << flush;
			break;
		}

	}

	cout << "dfpdsofpo\n";

	gatt.cb_connected();*/

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
