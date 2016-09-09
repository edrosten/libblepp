#include <blepp/lescan.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdlib>


using namespace BLEPP;
using namespace std;

#define check(X) do{\
if(!(X))\
{\
	cerr << "Test failed on line " << __LINE__ << ": " << #X << endl;\
	exit(1);\
}}while(0)

vector<uint8_t> to_data(const string& ss)
{
	istringstream s(ss);

	//Format is > 04 3E ...

	string tmp;
	s >> tmp;
	s>>hex;

	vector<uint8_t> ret;

	for(;;)
	{
		int x;
		s >> x;
		if(s.fail())
		{
			return ret;
		}
		else
			ret.push_back(x);
	}
	
}

int main()
{
	log_level = LogLevels::Trace;
/*
HCI sniffer - Bluetooth packet analyzer ver 2.5
device: hci0 snap_len: 1500 filter: 0xffffffffffffffff
< 01 0B 20 07 01 10 00 10 00 00 00 
> 04 0E 04 01 0B 20 00 
< 01 0C 20 02 01 01 
> 04 0E 04 01 0C 20 00 
> 04 3E 0C 02 01 04 01 0B 57 16 21 76 7C 00 C1 
< 01 0C 20 02 00 01 
> 04 0E 04 01 0C 20 00 
*/	

	AdvertisingResponse r = HCIScanner::parse_packet(to_data("> 04 3E 21 02 01 00 00 1B EE B5 80 07 00 15 02 01 06 11 06 64 97 81 D1 ED BA 6B AC 11 4C 9D 34 3E 20 09 73 BC")).back();
	check(r.UUIDs[0] == UUID("7309203e-349d-4c11-ac6b-baedd1819764"));
	check(r.UUIDs.size() == 1);
	check(r.service_data.empty());
	check(r.manufacturer_specific_data.empty());
	check(r.unparsed_data_with_types.empty());
	check(!r.local_name);
	check(!r.uuid_16_bit_complete);
	check(!r.uuid_32_bit_complete);
	check(!r.uuid_128_bit_complete);
	check(!r.flags->LE_limited_discoverable);
	check(r.flags);
	check(!r.flags->LE_limited_discoverable);
	check(r.flags->LE_general_discoverable);
	check(r.flags->BR_EDR_unsupported);
	check(!r.flags->simultaneous_LE_BR_controller);
	check(!r.flags->simultaneous_LE_BR_host);


	r = HCIScanner::parse_packet(to_data("> 04 3E 24 02 01 04 00 1B EE B5 80 07 00 18 17 09 44 79 6E 6F 66 69 74 20 49 6E 63 20 44 4F 54 53 20 78 78 78 78 31 BE")).back();
	check(r.UUIDs.size() == 0);
	check(r.service_data.empty());
	check(r.manufacturer_specific_data.empty());
	check(r.unparsed_data_with_types.empty());
	check(r.local_name);
	check(r.local_name->complete);
	check(r.local_name->name == "Dynofit Inc DOTS xxxx1");
	check(!r.uuid_16_bit_complete);
	check(!r.uuid_32_bit_complete);
	check(!r.uuid_128_bit_complete);
	check(!r.flags);

	r = HCIScanner::parse_packet(to_data("> 04 3E 17 02 01 00 01 0B 57 16 21 76 7C 0B 02 01 1A 07 FF 4C 00 10 02 0A 00 BC")).back();
	vector<uint8_t> vendor_data_1 = {0x4c, 0x00, 0x10, 0x02, 0x0a, 0x00};
	check(r.UUIDs.size() == 0);
	check(r.service_data.empty());
	check(r.manufacturer_specific_data.size() == 1);
	check(r.manufacturer_specific_data[0].size() == 6);
	check(equal(vendor_data_1.begin(), vendor_data_1.end(), r.manufacturer_specific_data[0].begin()));
	check(r.unparsed_data_with_types.empty());
	check(!r.local_name);
	check(r.flags);
	check(!r.flags->LE_limited_discoverable);
	check(r.flags->LE_general_discoverable);
	check(!r.flags->BR_EDR_unsupported);
	check(r.flags->simultaneous_LE_BR_controller);
	check(r.flags->simultaneous_LE_BR_host);

}
