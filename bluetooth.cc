
/*
   Copyright (c) 2013  Edward Rosten

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
		* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of the <organization> nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
