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

#ifndef __INC_LIBATTGATT_BLEDEVICE_H
#define __INC_LIBATTGATT_BLEDEVICE_H

#include <cstdint>
#include <vector>
#include <string>
#include <blepp/att_pdu.h>

namespace BLEPP
{

	//Almost zero resource to represent the ATT protocol on a BLE
	//device. This class does none of its own memory management, and will not generally allocate
	//or do other nasty things. Oh no, it allocates a buffer! FIXME!
	//
	//Mostly what it can do is write ATT command packets (PDUs) and receive PDUs back.
	struct BLEDevice
	{
		struct ReadError{};
		struct WriteError{};

		const int& sock;
		static const int buflen=ATT_DEFAULT_MTU;
		std::vector<std::uint8_t> buf;

		//template<class C> void test_fd_(int fd, int line);
		void test_pdu(int len);
		BLEDevice(const int& sock_);

		void send_read_request(std::uint16_t handle);
		void send_read_by_type(const bt_uuid_t& uuid, std::uint16_t start = 0x0001, std::uint16_t end=0xffff);
		void send_find_information(std::uint16_t start = 0x0001, std::uint16_t end=0xffff);
		void send_read_group_by_type(const bt_uuid_t& uuid, std::uint16_t start = 0x0001, std::uint16_t end=0xffff);
		void send_write_request(std::uint16_t handle, const std::uint8_t* data, int length);
		void send_write_request(std::uint16_t handle, std::uint16_t data);
		void send_handle_value_confirmation();
		void send_write_command(std::uint16_t handle, const std::uint8_t* data, int length);
		void send_write_command(std::uint16_t handle, std::uint16_t data);
		PDUResponse receive(std::uint8_t* buf, int max);
		PDUResponse receive(std::vector<std::uint8_t>& v);
	};

}

#endif
