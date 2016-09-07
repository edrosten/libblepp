#include <blepp/lescan.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cassert>


using namespace BLEPP;
using namespace std;


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
			cerr << ret.size() << endl;
			return ret;
		}
		else
			ret.push_back(x);
	}
	
}

int main()
{
	log_level = LogLevels::Trace;

	string data=R"(
HCI sniffer - Bluetooth packet analyzer ver 2.5
device: hci0 snap_len: 1500 filter: 0xffffffffffffffff
< 01 0B 20 07 01 10 00 10 00 00 00 
> 04 0E 04 01 0B 20 00 
< 01 0C 20 02 01 01 
> 04 0E 04 01 0C 20 00 
> 04 3E 17 02 01 00 01 0B 57 16 21 76 7C 0B 02 01 1A 07 FF 4C 
  00 10 02 0A 00 BC 
> 04 3E 0C 02 01 04 01 0B 57 16 21 76 7C 00 C1 
> 04 3E 24 02 01 04 00 1B EE B5 80 07 00 18 17 09 44 79 6E 6F 66 69 74 20 49 6E 63 20 44 4F 54 53 20 78 78 78 78 31 BE 
< 01 0C 20 02 00 01 
> 04 0E 04 01 0C 20 00 
)";

	AdvertisingResponse r = HCIScanner::parse_packet(to_data("> 04 3E 21 02 01 00 00 1B EE B5 80 07 00 15 02 01 06 11 06 64 97 81 D1 ED BA 6B AC 11 4C 9D 34 3E 20 09 73 BC")).back();

	assert(r.UUIDs[0] == UUID("7309203e-349d-4c11-ac6b-baedd1819764"));
	assert(r.UUIDs.size() == 1);
	assert(r.service_data.empty());
	assert(r.manufacturer_specific_data.empty());
	assert(r.unparsed_data_with_types.empty());
	assert(!r.local_name);
	assert(!r.uuid_16_bit_complete);
	assert(!r.uuid_32_bit_complete);
	assert(!r.uuid_128_bit_complete);
	assert(!r.flags->LE_limited_discoverable);
	assert(r.flags->LE_general_discoverable);
	assert(r.flags->BR_EDR_unsupported);
	assert(!r.flags->simultaneous_LE_BR_controller);
	assert(!r.flags->simultaneous_LE_BR_host);





}
