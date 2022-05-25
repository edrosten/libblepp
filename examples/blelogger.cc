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
#include <random>
#include <iterator>
#include <deque>
#include <cerrno>
#include <cstring>
#include <fstream>

// This is a slightly sketchy GNUPlot interface for realtime data vis from C++
#include "cxxgplot.h"
using namespace std;
using namespace chrono;
using namespace BLEPP;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// Compression algorithm from the FlexDot firmware. This is used for testing  
// purposes only in order to verify the correctness of the decompression algorithm
// see compress() below for the format of the packets
//
#define EMG_RAW_MODE_BUFFER_SIZE 20
#include <stdint.h>

typedef struct 
{
	uint8_t byte;
	uint8_t nybble;
	uint8_t write_buf;
	uint8_t sequence_number;
	uint8_t prev_val;
	uint8_t buffers[2][EMG_RAW_MODE_BUFFER_SIZE];
} NybbleBuffer;


NybbleBuffer compressed_data = {0, 0, 0, 0, 0, {{0}}};

void NB_zero()
{
	compressed_data.byte = 0;
	compressed_data.nybble = 0;
	compressed_data.prev_val=0;
}

void NB_push_nybble(uint8_t nybble)
{
	//Store high then low
	//The readon is that when bytes are printed, they go high,low, so this makes
	//thing easier to debug
	if(compressed_data.nybble == 0)
	{
		//Store the value clearing the lower bits
		compressed_data.buffers[compressed_data.write_buf][compressed_data.byte] = (nybble << 4) & 0xf0;
		compressed_data.nybble=1;
	}
	else
	{
		//Or in the bits, since the upper bits are clear.
		compressed_data.buffers[compressed_data.write_buf][compressed_data.byte++] |= (nybble & 0x0f);
		compressed_data.nybble=0;
	}
}

uint8_t* get_compresser_data()
{
	return compressed_data.buffers[!compressed_data.write_buf];
}

int get_compresser_data_size()
{
	return EMG_RAW_MODE_BUFFER_SIZE;
}

uint8_t compress(uint8_t v)
{
	//Statistical analysis shows the 15 most common values are [123,138)
	//These account for 75% of the values. So, implement nybble compression.
	//If the value is in the common range, emit val-123 as a nybble else
	//emit ff val
	//This gives 4*0.75 + 12*0.25 = 6 bits per sample, giving a 25% compression.
	//Should cut packet rate from 45 (too high, just) to 34.
	//Note there are 15 unused values, i.e. ff <123> and up.
	//Pad the packet with FF at the end, if it doesn't fit.
	//
	//Well, now. Now what I've got some actual complete data dumps, I can do a 
	//statistical analysis. With modular differencing, the most common numbers 15 are:
	//0,1,2,3,4,5,6,7 and 249,250,251,252,253,254,255
	//
	//The new packet format is as follows
	//first byte: actual value to be sent
	//remaining bytes, encode difference with nybble encoding above.
	//With 0-7 -> and 249->255 mapping to 8-15.
	//Using that encoding gives a 37.5% improvement (5 bits per sample)
	//cutting the packet rate from 45 to about 28
	
	
	uint8_t val = v - compressed_data.prev_val;
	uint8_t is_small = (val <= 7 || val >= 249);
	uint8_t nybbles = is_small?1:3;

	int buffer_size_in_nybbles = (EMG_RAW_MODE_BUFFER_SIZE-1)*2;
	int nybbles_so_far = compressed_data.byte*2 + compressed_data.nybble;

	int nybbles_remaining = buffer_size_in_nybbles - nybbles_so_far;

	int swap=0;

	//Check to see if we can send the byte, othewise flush the buffer.
	if(nybbles > nybbles_remaining)
	{
		if(nybbles_remaining == 0)
		{
			//Do nothing.
		}
		else if(nybbles_remaining == 1)
		{
			NB_push_nybble(0xf);
		}
		else if(nybbles_remaining == 2)
		{
			NB_push_nybble(0xf);
			NB_push_nybble(0xf);
		}
		else
		{
			//This can't happen
		}

		
		//Append the sequence number
		compressed_data.buffers[compressed_data.write_buf][EMG_RAW_MODE_BUFFER_SIZE-1] = compressed_data.sequence_number & 0xff;
		compressed_data.sequence_number++;

		//Swap the buffers
		compressed_data.write_buf ^= 1;

		//Reset
		NB_zero();
		
		swap=1;
	}

	//If we've flushed the buffer, then we want to send the first byte as-is.
	if(compressed_data.byte == 0 && compressed_data.nybble == 0)
	{
		NB_push_nybble((v >> 4) & 0xf);
		NB_push_nybble(v & 0xf);
	}
	else if(is_small)
	{		
		if(val <= 7)
			NB_push_nybble(val & 0xf);
		else
			NB_push_nybble((val -249 + 8) & 0xf);
	}
	else
	{
		NB_push_nybble(0xf);
		NB_push_nybble((val >> 4) & 0xf);
		NB_push_nybble(val & 0xf);
	}
	
	compressed_data.prev_val = v;

	return swap;
}

//
// End of the compression code
//


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// Decompression algorithm below
// 

// Index an array of bytes as nybbles
uint8_t get_nybble(const uint8_t* d, int nybble){
	uint8_t ch = d[nybble/2];
	uint8_t n;
	if(nybble %2 == 0 )
		n = ch >> 4;
	else
		n = ch & 0xf;
	return n;
}

// This function decompresses a single packet
vector<uint8_t> decompress(const vector<uint8_t> d)
{
	vector<uint8_t> ret;
	uint8_t v_0 = d[0]; //First byte is simply the value
	ret.push_back(v_0);

	int len = d.size();
	int nybbles = (len-2)*2; //Exclude the first byte (no encoding) and last (sequence number)

	int n_ind = 0;
	while(n_ind < nybbles){
		uint8_t nybble = get_nybble(d.data()+1, n_ind++);
		uint8_t dv;
		
		// 0xf is the flag value
		if(nybble == 0xf){
			// check for end condition
			if(n_ind +3 >= nybbles)
				break;
			
			// Otherwise the following two values encode the delta
			// In hindsight they may as well have encoded the value.
			uint8_t v_h = get_nybble(d.data()+1, n_ind++);
			uint8_t v_l = get_nybble(d.data()+1, n_ind++);
			dv = (v_h << 4) + v_l;
		}
		else{ //No flag: the nybble is simply the delta (using modular arithmetic)
			if(nybble <= 7)
				dv = nybble;
			else
				dv = nybble -8 + 249;

		}
		v_0 += dv;
		ret.push_back(v_0);
	}
	return ret;
}

// Ad-hoc test function. This generateds data with a Cauchy distribution (mostly small)
// and compresses it, then decompresses it and checks for agreement between the two
// streams
void test(){
	mt19937 engine;
	cauchy_distribution<> dist(128, 10);

	vector<uint8_t> original;
	vector<vector<uint8_t>> compressed;

	uint8_t seq=0;
	
	for(unsigned int i=0; i < 400; i++)
	{
		uint8_t data = dist(engine);
		uint8_t ready = compress(data);
		original.push_back(data);

		if(ready)
		{
		
			vector<uint8_t> packet;
			int read_buf = !compressed_data.write_buf;
			copy(begin(compressed_data.buffers[read_buf]), end(compressed_data.buffers[read_buf]), back_inserter(packet));
			packet.push_back(seq++);
			compressed.push_back(packet);
		}
	}

	vector<uint8_t> result;

	for(size_t i=0; i < 2+0*compressed.size(); i++){
		vector<uint8_t> d = decompress(compressed[i]);
		copy(d.begin(), d.end(), back_inserter(result));
	}

	for(size_t i=0; i < result.size(); i++){
		if(result[i] != original[i]){
			cout << "err " << i << "    " << result[i]+0 << ":" << original[i]+0 << endl;
			exit(1);
		}
	}
}






////////////////////////////////////////////////////////////////////////////////
//
// This program logs data from a flexdot. Provide the output file (which it will
// refust to overwrite), the BLE address and the capture mode. Note that raw mode
// is very expensive in terms of battery use and lower resolution.
// 
int main(int argc, char **argv)
{
	test();
	if(argc != 4)
	{	
		cerr << "Please supply address.\n";
		cerr << "Usage:\n";
		cerr << "prog <file> <address> <mode=raw|envelope>\n";
		exit(1);
	}
	
	
	std::string filename = argv[1];

	std::string mode = argv[3];
	if(mode != "raw" && mode != "envelope"){
		cerr << mode << " is not a valid mode\n";
		exit(1);
	}

	ifstream test(filename);
	if(test.is_open()){
		cerr << "File  " << filename << " already exists\n";
		exit(1);
	}
	ofstream output(filename);
	if(!output.is_open()){
		cerr << "File  " << filename << " failed: " << strerror(errno) << "\n";
		exit(1);
	}


	log_level = Info;

	
	BLEGATTStateMachine gatt;

	cplot::Plotter plot;
	deque<int> data; // Ringbuffer of data to plot


	std::function<void(const PDUNotificationOrIndication&)> notify_cb = [&](const PDUNotificationOrIndication& n)
	{
		//Callback to decode an envelope mode packet. The packet
		//consists of 7 16 bit envelope values a 1 4 byte sequence number and a 2
		//byte battery voltage. Battery voltage of -1 indicates no value, since it's
		//sampled only occasionally
		const uint8_t* d = n.value().first;

		// Decode the sequence number`
		int seq = ((d[17] * 16777216 + d[16]*65536 +d[15] *256 + d[14])>>0);
		
		// Decode the voltage
		int volt = ((0+d[19] *256 + d[18])>>0) ;
		double v=-1;
		if(volt != 0x8000)
			v = volt / 1000.0;

		output << "Packet " << seq << " " << v << "\n";

		// Decode the EMG signal
		int emg[7];
		for(int i=0; i < 7;i++)
		{
		    emg[i] = 0+d[i*2+1] *256 + d[i*2];
			output << "Envelope " << emg[i] << "\n";
		    data.push_back(emg[i]);
		}

		output << flush;
		
		cout << setprecision(15) << seq <<  " " << v << endl;

		while(data.size() > 1000)
			data.pop_front();
		
		plot.newline("line");
		plot.addpts(data);
		plot.draw();
	};

	std::function<void(const PDUNotificationOrIndication&)> raw_notify_cb = [&](const PDUNotificationOrIndication& n)
	{
		//Callback for a raw data packet
		vector<uint8_t> packet;
		copy(n.value().first, n.value().second, back_inserter(packet));

		vector<uint8_t> res = decompress(packet);
		copy(res.begin(), res.end(), back_inserter(data));
		
		// This is the sequence number
		cout << "Packet " << 0+packet.back() << endl;
		output << "Packet " << 0+packet.back() << endl;
		for(const auto& r:res){
			output << "voltage " << 0+r << endl;
			data.push_back(r);

		}

		while(data.size() > 10000)
			data.pop_front();
		
		plot.newline("line");
		plot.addpts(data);
		plot.draw();
		output << flush;
	};

	
	// BLE device scan callback: when the BLE system queries all the 
	// features of the device, this callback is run
	std::function<void()> cb = [&gatt, &raw_notify_cb, &notify_cb, &mode](){
		pretty_print_tree(gatt);

		for(auto& service: gatt.primary_services)
			for(auto& characteristic: service.characteristics)
				if(characteristic.uuid == UUID("e5f49879-6ee1-479e-bfec-3d35e13d3b88") && mode == "envelope")
				{
					characteristic.cb_notify_or_indicate = notify_cb;
					characteristic.set_notify_and_indicate(true, false);
				}
				else if(characteristic.uuid == UUID("001785a0-cf2e-47f5-9d43-1217696f8ef9") && mode == "raw")
				{
					characteristic.cb_notify_or_indicate = raw_notify_cb;
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
	gatt.connect_blocking(argv[2]);
	for(;;)
		gatt.read_and_process_next();

}
