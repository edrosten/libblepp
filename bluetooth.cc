#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#include <sys/socket.h>

#include <unistd.h>
#include "logging.h"

using namespace std;


void test_fd_(int fd, int line)
{
	if(fd < 0)
	{
		cerr << "Error on line " << line << ": " <<strerror(errno) << endl;
		exit(1);
	}

	cerr << "Line " << line << " ok\n";

}

#define test(X) test_fd_(X, __LINE__)

struct BLEDevice
{
	bool constructing;
	int sock;

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
			LOG(Debug, "System call on " << line << ": " << strerror(errno));
	}

	BLEDevice();

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
		
	bdaddr_t source_address = {0};  //i.e. the adapter. Note, 0, corresponds to BDADDR_ANY
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

int main(int argc, char **argv)
{
	log_level = Trace;

	BLEDevice b;
}
