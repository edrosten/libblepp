#include "lib/uuid.h"
#include "att_pdu.h"


namespace ATT
{


string to_hex(const uint16_t& u)
{
	ostringstream os;
	os << setw(4) << setfill('0') << hex << u;
	return os.str();
}

string to_hex(const uint8_t& u)
{
	ostringstream os;
	os << setw(2) << setfill('0') << hex << (int)u;
	return os.str();
}

string to_str(const uint8_t& u)
{
	if(u < 32 || u > 126)
		return "\\x" + to_hex(u);
	else
	{
		char buf[] = {(char)u, 0};
		return buf;
	}
}

string to_str(const bt_uuid_t& uuid)
{
	//8 4 4 4 12
	if(uuid.type == BT_UUID16)
		return to_hex(uuid.value.u16);
	else if(uuid.type == BT_UUID128)
		return "--128--";
	else
		return "uuid.wtf";

}

string to_hex(const uint8_t* d, int l)
{
	ostringstream os;
	for(int i=0; i < l; i++)
		os << to_hex(d[i]) << " ";
	return os.str();
}
string to_hex(pair<const uint8_t*, int> d)
{
	return to_hex(d.first, d.second);
}

string to_hex(const vector<uint8_t>& v)
{
	return to_hex(v.data(), v.size());
}

string to_str(const uint8_t* d, int l)
{
	ostringstream os;
	for(int i=0; i < l; i++)
		os << to_str(d[i]);
	return os.str();
}
string to_str(pair<const uint8_t*, int> d)
{
	return to_str(d.first, d.second);
}
string to_str(pair<const uint8_t*, const uint8_t*> d)
{
	return to_str(d.first, d.second - d.first);
}

string to_str(const vector<uint8_t>& v)
{
	return to_str(v.data(), v.size());
}




}
