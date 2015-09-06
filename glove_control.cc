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
#include <chrono>
#include <libattgatt/blestatemachine.h>
#include <libattgatt/float.h>
#include <deque>
#include <sys/time.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

void bin(uint8_t i)
{
	for(int b=7; b >= 0; b--)
		cout << !!(i & (1<<b));

}

//ASCII throbber
string throbber(int i)
{
	string base = " (--------------------)";
	
	i = i%40;
	if(i >= 20)
		i = 19-(i-20);
	base[i + 2] = 'O';

	return base + string(base.size(), '\b');
}


double get_time_of_day()
{
        struct timeval tv;
        gettimeofday(&tv,NULL);
        return tv.tv_sec+tv.tv_usec * 1e-6;
}

void sane()
{
	system("stty sane");
}

int main(int argc, char **argv)
{
	if(argc != 2)
	{	
		cerr << "Please supply address.\n";
		cerr << "Usage:\n";
		cerr << "prog address\n";
		exit(1);
	}

	atexit(sane);
	system("stty min 1 time 0 -icanon -echo");

	log_level = Info;

	
	//This is the interface to the BLE protocol.
	BLEGATTStateMachine gatt;
	
	auto motor_A_UUID = UUID("ec082d84-17a3-4ce4-a6c2-4121ea9a6d25");
	auto motor_B_UUID = UUID("ec082d84-17a3-4ce4-a6c2-4121ea9a6d26");
	auto set_zero_UUID = UUID("ec082d84-17a3-4ce4-a6c2-4121ea9a6d27");
	auto read_motor_UUID = UUID("ec082d84-17a3-4ce4-a6c2-4121ea9a6d28");
	auto save_and_activate_UUID = UUID("ec082d84-17a3-4ce4-a6c2-4121ea9a6d29");

	Characteristic* motor_A= 0;
	Characteristic* motor_B= 0;
	Characteristic* set_zero= 0;
	Characteristic* read_motor= 0;
	Characteristic* save_and_activate= 0;

	std::function<void()> cb = [&](){

		pretty_print_tree(gatt);

		for(auto& service: gatt.primary_services)
			if(service.uuid == UUID("8eb5ed87-b50b-4605-b6ae-395db35712c2"))
				for(auto& characteristic: service.characteristics)
					if(characteristic.uuid == motor_A_UUID)
						motor_A = &characteristic;
					else if(characteristic.uuid == motor_B_UUID)
						motor_B = &characteristic;
					else if(characteristic.uuid == set_zero_UUID)
						set_zero = &characteristic;
					else if(characteristic.uuid == read_motor_UUID)
						read_motor = &characteristic;
					else if(characteristic.uuid == save_and_activate_UUID)
						save_and_activate = &characteristic;
	};
	
	////////////////////////////////////////////////////////////////////////////////
	//
	// This is somewhat important.  Set up callback for disconnection
	//
	// All reasonable errors are handled by a disconnect. The BLE spec specifies that
	// if the device sends invalid data, then the client must disconnect.
	//
	// Failure to connect also comes here.
	gatt.cb_disconnected = [](BLEGATTStateMachine::Disconnect d)
	{
		cerr << "Disconnect for reason " << BLEGATTStateMachine::get_disconnect_string(d) << endl;
		exit(1);
	};
	
	bool can_write=1;
	gatt.cb_write_response = [&]()
	{
		can_write=1;
	};

	gatt.setup_standard_scan(cb);

	int motor_a_pos = 0;
	int motor_b_pos = 0;

	int a_retracted = 10;
	int b_retracted = 20;
		
	const int Idle=0;
	const int SendThumbZeroNext=1;
	const int SendThumbRetractNext = 2;
	const int WaitForClose=3;
	const int WaitForFinger=4;
	
	int state =0;
	steady_clock::time_point open_time, thumb_close_time;

	const auto time_until_close = seconds(5);
	const auto finger_delay = seconds(2);

	////////////////////////////////////////////////////////////////////////////////
	try
	{ 
		//Connect as a non blocking call
		gatt.connect_nonblocking(argv[1]);



		//Example of how to use the state machine with select()
		//
		//This just demonstrates the capability and should be easily
		//transferrable to poll(), epoll(), libevent and so on.
		fd_set write_set, read_set;

		for(int i=0;;i++)
		{
			FD_ZERO(&read_set);
			FD_ZERO(&write_set);

			//Reads are always a possibility due to asynchronus notifications.
			FD_SET(gatt.socket(), &read_set);
			//Listen on stdin as well
			FD_SET(0, &read_set);

			//Writes are usually available, so only check for them when the 
			//state machine wants to write.
			if(gatt.wait_on_write())
				FD_SET(gatt.socket(), &write_set);


			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 10000;
			int result = select(gatt.socket() + 1, &read_set, &write_set, NULL, & tv);

			if(FD_ISSET(gatt.socket(), &write_set))
				gatt.write_and_process_next();

			if(FD_ISSET(gatt.socket(), &read_set))
				gatt.read_and_process_next();
			
			if(FD_ISSET(0, &read_set))
			{	
				char c;
				read(0, &c, 1);

				cout << "Read character " << c << "\r\n" << flush;

				if(!gatt.wait_on_write() && can_write)
				{
					if(c == 'q')
					{
						motor_a_pos++;
						motor_A->write_request((uint8_t)motor_a_pos);
						can_write=0;
					}
					else if(c == 'a')
					{
						motor_a_pos--;
						motor_A->write_request((uint8_t)motor_a_pos);
						can_write=0;
					}
					else if(c == 'e')
					{
						motor_b_pos++;
						motor_B->write_request((uint8_t)motor_b_pos);
						can_write=0;
					}
					else if(c == 'd')
					{
						motor_b_pos--;
						motor_B->write_request((uint8_t)motor_b_pos);
						can_write=0;
					}
					else if(c == 'z')
					{
						set_zero->write_request((uint8_t)motor_b_pos);
						motor_a_pos = 0;
						motor_b_pos = 0;
						can_write=0;
					}
					else if(c == 's')
					{
						a_retracted = motor_a_pos;
						b_retracted = motor_b_pos;

						motor_A->write_request(0);
						motor_a_pos = 0;
						state = SendThumbZeroNext;
						can_write=0;
					}
					else if(c == ' ')
					{
						motor_A->write_request(a_retracted);
						motor_a_pos = a_retracted;
						state = SendThumbRetractNext;
						can_write=0;
					}
				}
			}

			if(can_write && state == SendThumbZeroNext)
			{
				cout << "Issuing thumb to zero\n";
				motor_B->write_request(0);
				motor_b_pos = 0;
				state = Idle;
				can_write=0;
			}
			else if(can_write && state == SendThumbRetractNext)
			{
				cout << "Issuing thumb retract\n";
				motor_B->write_request(b_retracted);
				motor_b_pos = b_retracted;
				state = WaitForClose;
				can_write=0;
				open_time=steady_clock::now();
			}
			else if(can_write && state == WaitForClose)
			{
				if(steady_clock::now() - open_time > time_until_close)
				{
					cout << "Issuing thumb close\n";
					motor_B->write_request(0);
					motor_b_pos = 0;
					state = WaitForFinger;
					can_write=0;
					thumb_close_time=steady_clock::now();
				}
			}
			else if(can_write && state == WaitForFinger)
			{
				if(steady_clock::now() - thumb_close_time > finger_delay)
				{
					cout << "Issuing finger close\n";
					motor_A->write_request(0);
					motor_a_pos = 0;
					state = Idle;
					can_write=0;
				}
			}




			cout << throbber(i) << flush;
			/*
			   if(i % 100 == 0 && gatt.is_idle())
			   {
			   enable = !enable;
			   cb();
			   }*/

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
