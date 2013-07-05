#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#include <sys/socket.h>

#include <unistd.h>

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

#define test_fd(X) test_fd_(X, __LINE__)

int main(int argc, char **argv)
{
	//Allocate socket and create endpoint.
	int sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	test_fd(sock);

	//Bind the socket
	//I believe that l2 is for an l2cap socket. These are kind of like
	//UDP in that they have port numbers and are packet oriented.
	struct sockaddr_l2 addr;
		
	bdaddr_t source_address; //i.e. the adapter.
	source_address = *BDADDR_ANY;
	
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
	addr.l2_cid = htobs(4); //HAXXY


	bacpy(&addr.l2_bdaddr, &source_address);

	//Address type: Low Energy public
	addr.l2_bdaddr_type=BDADDR_LE_PUBLIC;
	
	//Bind socket. This associates it with a particular adapter.
	//We ose ANY as the source address, so packets will go out of 
	//whichever adapter necessary.
	int ret = bind(sock, (sockaddr*)&addr, sizeof(addr));
	test_fd(ret);

	
	//Need to do l2cap_set here
	
	//Can also use bacpy to copy addresses about
	str2ba("3C:2D:B7:85:50:2A", &addr.l2_bdaddr);
	ret = connect(sock, (sockaddr*)&addr, sizeof(addr));
	test_fd(ret);


	//And this seems to work up to here.

}
