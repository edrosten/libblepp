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
#include "cxxgplot.h"
using namespace std;

void bin(uint8_t i)
{
	for(int b=7; b >= 0; b--)
		cout << !!(i & (1<<b));

}


int main(int argc, char **argv)
{
	if(argc != 2)
	{	
		cerr << "Please supply address.\n";
		exit(1);
	}

	log_level = Warning;

	BLEGATTStateMachine gatt(argv[1]);
	
	cplot::Plotter plot;

	plot.range = " [ ] [0:1000] ";

	deque<int> points;

	std::function<void(const PDUNotificationOrIndication&)> notify_cb = [&](const PDUNotificationOrIndication& n)
	{
		//cerr << "Hello: "  << ((n.value().first[0] + (n.value().first[1]<<8))>>4) << endl;
		//cerr << "Hello: "  << hex  << setfill('0') << setw(4) << ((0+n.value().first[1] *256 + n.value().first[0])>>0) << dec << endl;

		//for(const uint8_t* a = n.value().first; a < n.value().second; a++)
		//	cout << to_hex(*a) << " ";
		//cout << endl;

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



	std::function<void()> cb = [&gatt, &notify_cb](){
		for(auto& service: gatt.primary_services)
			for(auto& characteristic: service.characteristics)
				if(service.uuid == UUID("7309203e-349d-4c11-ac6b-baedd1819764") && characteristic.uuid == UUID("53f72b8c-ff27-4177-9eee-30ace844f8f2"))
				{
					characteristic.cb_notify_or_indicate = notify_cb;
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
