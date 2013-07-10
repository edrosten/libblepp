#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cassert>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#include <sys/socket.h>

#include <unistd.h>
#include "logging.h"
#include "lib/uuid.h"
extern "C"{
#include "att.h"
}
using namespace std;


void test_fd_(int fd, int line)
{
	if(fd < 0)
	{
		cerr << "Error on line " << line << ": " <<strerror(errno) << endl;
		exit(1);
	}

	cerr << "Line " << line << " ok = " << fd  << endl;

}

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

#define test(X) test_fd_(X, __LINE__)
class ResponsePDU;

class ResponsePDU
{
	protected:
		void type_mismatch() const
		{	
			throw 1;
		}
	
	public:

		const uint8_t* data;
		int length;

		uint8_t uint8(int i) const
		{
			assert(i >= 0 && i < length);
			return data[i];
		}

		uint16_t uint16(int i) const
		{
			return uint8(i) | (uint8(i+1) << 8);
		}

		ResponsePDU(const uint8_t* d_, int l_)
		:data(d_),length(l_)
		{
		}

		uint8_t type() const 
		{
			return uint8(0);
		}
};

class PDUErrorResponse: public ResponsePDU
{
	public:
		PDUErrorResponse(const ResponsePDU& p_)
		:ResponsePDU(p_)
		{
			if(type()  != ATT_OP_ERROR)
				type_mismatch();
		}

		uint8_t request_opcode() const
		{
			return uint8(2);
		}

		uint16_t handle() const
		{
			return uint16(2);
		}

		uint8_t error_code() const
		{
			return uint8(4);
		}
		
		const char* error_str() const
		{
			return att_ecode2str(error_code());
		}
};


class PDUReadByTypeResponse: public ResponsePDU
{
	public:
		PDUReadByTypeResponse(const ResponsePDU& p_)
		:ResponsePDU(p_)
		{
			if(type()  != ATT_OP_READ_BY_TYPE_RESP)
				type_mismatch();

			if((length - 1) % element_size() != 0)
			{
				//Packet length is invalid.
				//throw 1.;
			}
		}

		int value_size() const
		{
			return uint8(1) -2;
		}

		int element_size() const
		{
			return uint8(1);
		}

		int num_elements() const
		{
			return (length - 1) / element_size();
		}
		
		uint16_t handle(int i) const
		{
			return uint16(i*element_size() + 2);
		}

		pair<const uint8_t*, int> value(int i) const
		{
			return make_pair(data + i*element_size() + 4, value_size());
		}

		uint16_t value_uint16(int i) const
		{
			assert(value_size() == 2);
			return uint16(i*element_size()+4);
		}

};

void pretty_print(const ResponsePDU& pdu)
{
	if(log_level >= Debug)
	{
		cerr << "debug: ---PDU packet ---\n";
		cerr << "debug: " << to_hex(pdu.data, pdu.length) << endl;
		cerr << "debug: " << to_str(pdu.data, pdu.length) << endl;
		cerr << "debug: Packet type: " << to_hex(pdu.type()) << " " << att_op2str(pdu.type()) << endl;
		
		if(pdu.type() == ATT_OP_ERROR)
			cerr << "debug: " << PDUErrorResponse(pdu).error_str() << " in response to " <<  att_op2str(PDUErrorResponse(pdu).request_opcode()) << " on handle " + to_hex(PDUErrorResponse(pdu).handle());
		else if(pdu.type() == ATT_OP_READ_BY_TYPE_RESP)
		{
			PDUReadByTypeResponse p(pdu);

			cerr << "debug: elements = " << p.num_elements() << endl;
			cerr << "debug: value size = " << p.value_size() << endl;

			for(int i=0; i < p.num_elements(); i++)
			{
				cerr << "debug: " << to_hex(p.handle(i)) << " ";
				if(p.value_size() != 2)
					cerr << "-->" << to_str(p.value(i)) << "<--" << endl;
				else
					cerr << to_hex(p.value_uint16(i)) << endl;
			}

		}
		
		cerr << "debug:\n";
	}
};

struct BLEDevice
{
	bool constructing;
	int sock;
	static const int buflen=1024;

	void test_fd_(int fd, int line)
	{
		if(fd < 0)
		{
			LOG(Info, "Error on line " << line << ": " <<strerror(errno));

			if(constructing && sock >= 0)
			{
				int ret = close(sock);
				if(ret < 0)
					LOG(Warning, "Error on line " << line << ": " <<strerror(errno));
				else
					LOG(Debug, "System call on " << line << ": " << strerror(errno));
			}
			exit(1);
		}
		else
			LOG(Debug, "System call on " << line << ": " << strerror(errno) << " ret = " << fd);
	}

	BLEDevice();

	void send_read_by_type(const bt_uuid_t& uuid, uint16_t start = 0x0001, uint16_t end=0xffff)
	{
		vector<uint8_t> buf(buflen);
		int len = enc_read_by_type_req(start, end, const_cast<bt_uuid_t*>(&uuid), buf.data(), buf.size());
		int ret = write(sock, buf.data(), len);
		test(ret);
	}

	void send_find_information(uint16_t start = 0x0001, uint16_t end=0xffff)
	{
		vector<uint8_t> buf(buflen);
		int len = enc_find_info_req(start, end, buf.data(), buf.size());
		int ret = write(sock, buf.data(), len);
		test(ret);
	}

	void send_read_group_by_type(const bt_uuid_t& uuid, uint16_t start = 0x0001, uint16_t end=0xffff)
	{
		vector<uint8_t> buf(buflen);
		int len = enc_read_by_grp_req(start, end, const_cast<bt_uuid_t*>(&uuid), buf.data(), buf.size());
		int ret = write(sock, buf.data(), len);
		test(ret);
	}

	ResponsePDU receive(uint8_t* buf, int max)
	{
		int len = read(sock, buf, max);
		test(len);
		pretty_print(ResponsePDU(buf, len));
		return ResponsePDU(buf, len);
	}

	ResponsePDU receive(vector<uint8_t>& v)
	{
		return receive(v.data(), v.size());
	}
};

template<class C> const C& haxx(const C& X)
{
	return X;
}

int haxx(uint8_t X)
{
	return X;
}
#define LOGVAR(X) LOG(Debug, #X << " = " << haxx(X))

BLEDevice::BLEDevice()
:constructing(true)
{
	//Allocate socket and create endpoint.
	sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	test(sock);
	
	////////////////////////////////////////
	//Bind the socket
	//I believe that l2 is for an l2cap socket. These are kind of like
	//UDP in that they have port numbers and are packet oriented.
	struct sockaddr_l2 addr;
		
	bdaddr_t source_address = {{0,0,0,0,0,0}};  //i.e. the adapter. Note, 0, corresponds to BDADDR_ANY
	                                //However BDADDR_ANY uses a nonstandard C hack and does not compile
									//under C++. So, set it manually.
	//So, a sockaddr_l2 has the family (obviously)
	//a PSM (wtf?) 
	//  Protocol Service Multiplexer (WTF?)
	//an address (of course)
	//a CID (wtf) 
	//  Channel ID (i.e. port number?)
	//and an address type (wtf)

	memset(&addr, 0, sizeof(addr));
	addr.l2_family = AF_BLUETOOTH;

	addr.l2_psm = 0;
	addr.l2_cid = htobs(4); //HAXXY FIXME?


	bacpy(&addr.l2_bdaddr, &source_address);

	//Address type: Low Energy public
	addr.l2_bdaddr_type=BDADDR_LE_PUBLIC;
	
	//Bind socket. This associates it with a particular adapter.
	//We chose ANY as the source address, so packets will go out of 
	//whichever adapter necessary.
	int ret = bind(sock, (sockaddr*)&addr, sizeof(addr));
	test(ret);
	

	////////////////////////////////////////
	
	//Need to do l2cap_set here
	l2cap_options options;
	unsigned int len = sizeof(options);
	memset(&options, 0, len);

	//Get the options with a minor bit of cargo culting.
	//SOL_L2CAP seems to mean that is should operate at the L2CAP level of the stack
	//L2CAP_OPTIONS who knows?
	ret = getsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, &options, &len);
	test(ret);

	LOGVAR(options.omtu);
	LOGVAR(options.imtu);
	LOGVAR(options.flush_to);
	LOGVAR(options.mode);
	LOGVAR(options.fcs);
	LOGVAR(options.max_tx);
	LOGVAR(options.txwin_size);


	
	//Can also use bacpy to copy addresses about
	str2ba("3C:2D:B7:85:50:2A", &addr.l2_bdaddr);
	ret = connect(sock, (sockaddr*)&addr, sizeof(addr));
	test(ret);


	//And this seems to work up to here.
}

LogLevels log_level;

int main(int , char **)
{
	log_level = Trace;
	vector<uint8_t> buf(256);

	BLEDevice b;
	
	bt_uuid_t uuid;
	uuid.type = BT_UUID16;

	uuid.value.u16 = 0x2800;
	b.send_read_by_type(uuid);
	b.receive(buf);

	uuid.value.u16 = 0x2901;
	b.send_read_by_type(uuid);
	b.receive(buf);

	uuid.value.u16 = 0x2803;
	b.send_read_by_type(uuid);
	b.receive(buf);

	b.send_read_by_type(uuid, 0x0008);
	b.receive(buf);
	b.send_read_by_type(uuid, 0x0010);
	b.receive(buf);
	b.send_read_by_type(uuid, 0x0021);
	b.receive(buf);


	cerr << endl<<endl<<endl;

	b.send_find_information();
	b.receive(buf);

	cerr << endl<<endl<<endl;
	cerr << endl<<endl<<endl;
	cerr << endl<<endl<<endl;
	uuid.value.u16 = 0x2800;
	b.send_read_by_type(uuid);
	b.receive(buf);
	b.send_read_group_by_type(uuid);
	b.receive(buf);
	b.send_read_group_by_type(uuid, 0x0013);
	b.receive(buf);
}
