#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>

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

#define test(X) test_fd_(X, __LINE__)

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

	void send_read_characteristic_by_uuid(const bt_uuid_t& uuid, uint16_t start = 0x0001, uint16_t end=0xffff)
	{
		vector<uint8_t> buf(buflen);
		int len = enc_read_by_type_req(start, end, const_cast<bt_uuid_t*>(&uuid), buf.data(), buf.size());
		int ret = write(sock, buf.data(), len);
		test(ret);
		for(int i=0; i < len; i++)
			printf("%02x ", buf[i]);
		cout << endl;


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

	BLEDevice b;
	
	bt_uuid_t uuid;
	uuid.type = BT_UUID16;
	uuid.value.u16 = 0x2800;

	b.send_read_characteristic_by_uuid(uuid);

	char buf[1024];
	int x;

	x = read(b.sock, buf, 1024);
	test(x);

	for(int i=0; i < x; i++)
		printf("%02x ", 0xff&buf[i]);
	cout << endl;

	uuid.value.u16 = 0x2901;
	b.send_read_characteristic_by_uuid(uuid);

	x = read(b.sock, buf, 1024);
	test(x);

	for(int i=0; i < x; i++)
		printf("%02x ", 0xff&buf[i]);
	cout << endl;
}
