/*
 *
 *  blepp - Implementation of the Generic ATTribute Protocol
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
#include "blepp/bledevice.h"
#include "blepp/logging.h"
#include "blepp/att_pdu.h"


#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#include <sys/socket.h>

#include <unistd.h>

using namespace std;

namespace BLEPP
{

	template<class C> const C& haxx(const C& X)
	{
		return X;
	}

	inline int haxx(uint8_t X)
	{
		return X;
	}

	#define test(X, Y) test_fd_<BLEDevice::Y##Error>(X, __LINE__)
	#define TEST(X, S, B, L) call(X(),S, B, L)

	template<class C> void test_fd_(int fd, int line)
	{
		if(fd < 0)
		{
			LOG(Info, "Error on line " << line << "( " << __FILE__ << "): " <<strerror(errno));
			throw C();
		}
		else
			LOG(Debug, "System call on " << line << "( " << __FILE__ << "): " << strerror(errno) << " ret = " << fd);
	}

	void BLEDevice::test_pdu(int len)
	{
		if(len == 0)
			throw logic_error("Error constructing packet");
	}

	class Read{};
	class Write{};

	void call(const Write&, int sock, const uint8_t* buf, size_t len, int line)
	{
		test_fd_<BLEDevice::WriteError>(write(sock, buf, len), line);
	}


	void call(const Read&, int sock, uint8_t* buf, size_t len, int line)
	{
		test_fd_<BLEDevice::WriteError>(read(sock, buf, len), line);
	}

	void BLEDevice::send_read_request(uint16_t handle)
	{
		int len = enc_read_req(handle, buf.data(), buf.size());
		test_pdu(len);
		int ret = write(sock, buf.data(), len);
		test(ret, Write);
	}

	void BLEDevice::send_read_by_type(const bt_uuid_t& uuid, uint16_t start, uint16_t end)
	{
		int len = enc_read_by_type_req(start, end, const_cast<bt_uuid_t*>(&uuid), buf.data(), buf.size());
		test_pdu(len);
		int ret = write(sock, buf.data(), len);
		test(ret, Write);
	}

	void BLEDevice::send_find_information(uint16_t start, uint16_t end)
	{
		int len = enc_find_info_req(start, end, buf.data(), buf.size());
		test_pdu(len);
		int ret = write(sock, buf.data(), len);
		test(ret, Write);
	}

	void BLEDevice::send_read_group_by_type(const bt_uuid_t& uuid, uint16_t start, uint16_t end)
	{
		int len = enc_read_by_grp_req(start, end, const_cast<bt_uuid_t*>(&uuid), buf.data(), buf.size());
		test_pdu(len);
		int ret = write(sock, buf.data(), len);
		test(ret, Write);
	}

	void BLEDevice::send_write_request(uint16_t handle, const uint8_t* data, int length)
	{
		int len = enc_write_req(handle, data, length, buf.data(), buf.size());
		test_pdu(len);
		int ret = write(sock, buf.data(), len);
		test(ret, Write);
	}

	void BLEDevice::send_write_request(uint16_t handle, uint16_t data)
	{
		const uint8_t buf[2] = { (uint8_t)(data & 0xff), (uint8_t)((data & 0xff00) >> 8)};
		send_write_request(handle, buf, 2);
	}

	void BLEDevice::send_handle_value_confirmation()
	{
		int len = enc_confirmation(buf.data(), buf.size());
		test_pdu(len);
		int ret = write(sock, buf.data(), len);
		test(ret, Write);
	}

	void BLEDevice::send_write_command(uint16_t handle, const uint8_t* data, int length)
	{
		int len = enc_write_cmd(handle, data, length, buf.data(), buf.size());
		test_pdu(len);
		int ret = write(sock, buf.data(), len);
		test(ret, Write);
	}

	void BLEDevice::send_write_command(uint16_t handle, uint16_t data)
	{
		const uint8_t buf[2] = { (uint8_t)(data & 0xff), (uint8_t)((data & 0xff00) >> 8)};
		send_write_command(handle, buf, 2);
	}


	PDUResponse BLEDevice::receive(uint8_t* buf, int max)
	{
		int len = read(sock, buf, max);
		test(len, Read);
		pretty_print(PDUResponse(buf, len));
		return PDUResponse(buf, len);
	}

	PDUResponse BLEDevice::receive(vector<uint8_t>& v)
	{
		return receive(v.data(), v.size());
	}




	BLEDevice::BLEDevice(const int& sock_)
	:sock(sock_)
	{
	/*
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
		addr.l2_cid = htobs(LE_ATT_CID);


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
		str2ba(address.c_str(), &addr.l2_bdaddr);
		ret = connect(sock, (sockaddr*)&addr, sizeof(addr));
		test(ret);


		//And this seems to work up to here.

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
	*/
		buf.resize(ATT_DEFAULT_MTU);
	}

}
