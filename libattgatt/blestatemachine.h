#ifndef __INC_LIBATTGATT_BLESTATEMACHINE_H
#define __INC_LIBATTGATT_BLESTATEMACHINE_H
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
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <tuple>
#include <stdexcept>
#include <functional>
#include <algorithm>

#include  <libattgatt/logging.h>
#include  <libattgatt/bledevice.h>
#include <libattgatt/att_pdu.h>
#include <libattgatt/pretty_printers.h>
using namespace std;


///Interpret a ReadByTypeResponse packet as a ReadCharacteristic packet
class GATTReadCharacteristic: public  PDUReadByTypeResponse
{
	public:
	struct Characteristic
	{
		uint16_t handle;
		uint8_t  flags;
		bt_uuid_t uuid;
	};

	GATTReadCharacteristic(const PDUResponse& p)
	:PDUReadByTypeResponse(p)
	{
		if(value_size() != 5 && value_size() != 19)		
			throw runtime_error("Invalid packet size in GATTReadCharacteristic");
	}

	Characteristic characteristic(int i) const
	{
		Characteristic c;
		c.handle = att_get_u16(value(i).first + 1);
		c.flags  = value(i).first[0];

		if(value_size() == 5)
			c.uuid= att_get_uuid16(value(i).first + 3);
		else
			c.uuid= att_get_uuid128(value(i).first + 3);

		return c;
	}
};



///Interpret a ReadByTypeResponse packet as a ReadCharacteristic packet
class GATTReadCCC: public  PDUReadByTypeResponse
{
	public:

	GATTReadCCC(const PDUResponse& p)
	:PDUReadByTypeResponse(p)
	{
		if(value_size() != 2)
			throw runtime_error("Invalid packet size in GATTReadCharacteristic");
	}

	uint16_t ccc(int i) const
	{
		return att_get_u16(value(i).first);
	}
};

///Interpret a read_group_by_type resoponde as a read service group response
class GATTReadServiceGroup: public PDUReadGroupByTypeResponse
{
	public:
	GATTReadServiceGroup(const PDUResponse& p)
	:PDUReadGroupByTypeResponse(p)
	{
		if(value_size() != 2 && value_size() != 16)
		{
			LOG(Error, "UUID length" << value_size());
			error<std::runtime_error>("Invalid UUID length in PDUReadGroupByTypeResponse");
		}
	}

	bt_uuid_t uuid(int i) const
	{
		const uint8_t* begin = data + i*element_size() + 6;

		bt_uuid_t uuid;
		if(value_size() == 2)
		{
			uuid.type = BT_UUID16;
			uuid.value.u16 = att_get_u16(begin);
		}
		else
		{
			uuid.type = BT_UUID128;
			uuid.value.u128 = att_get_u128(begin);
		}
			
		return uuid;
	}
};

class BLEGATTStateMachine;

enum  States
{
	Idle,
	ReadingPrimaryService,
	FindAllCharacteristics,
	GetClientCharaceristicConfiguration,
	AwaitingWriteResponse,
};

static const int Waiting=-1;


class UUID: public bt_uuid_t
{
	public:

	explicit UUID(const uint16_t& u)
	{
		type = BT_UUID16;
		value.u16 = u;
	}
	
	UUID(){}

	UUID(const UUID&) = default;

	static UUID from(const bt_uuid_t& uuid)
	{
		UUID ret;
		(bt_uuid_t&)ret = uuid;

		return ret;
	}

	bool operator==(const UUID& uuid) const
	{
		return bt_uuid_cmp(this, &uuid) == 0;
	}
};


struct Characteristic
{	
	private:
	BLEGATTStateMachine* s;

	public:

	Characteristic(BLEGATTStateMachine* s_)
	:s(s_)
	{}

	void set_notify_and_indicate(bool , bool);
	std::function<void(const PDUNotificationOrIndication&)> cb_notify_or_indicate;

	//Flags indicating various properties
	bool broadcast, read, write_without_response, write, notify, indicate, authenticated_write, extended;

	//UUID, i.e. name of what the characteristic represents semantically
	UUID uuid;

	//Where the value can be read/written
	uint16_t value_handle;

	//Where we write to configure, i.e. set notify and/or indicate
	//0 means invalid.
	uint16_t client_characteric_configuration_handle;
	uint16_t ccc_last_known_value;
	
	uint16_t first_handle, last_handle;
};

struct StateMachineGoneBad: public runtime_error
{
	StateMachineGoneBad(const std::string& err)
	:runtime_error(err)
	{
	}
};


struct ServiceInfo
{
	string name, id;
	UUID uuid;
};

struct PrimaryService
{
	uint16_t start_handle;
	uint16_t end_handle;
	UUID uuid;
	vector<Characteristic> characteristics;
};


const ServiceInfo* lookup_service_by_UUID(const UUID& uuid);

class BLEGATTStateMachine
{
	private:

		static void buggerall();

		BLEDevice dev;
		

		States state = Idle;
		int next_handle_to_read=-1;
		int last_request=-1;
		
		std::vector<std::uint8_t> buf;


		struct PrimaryServiceInfo
		{
			string name, id;
			UUID uuid;
		};

		void reset();
		void state_machine_write();

	public:

		std::vector<PrimaryService> primary_services;

		std::function<void()> cb_connected = buggerall;
		std::function<void()> cb_services_read = buggerall;
		std::function<void()> cb_notify = buggerall;
		std::function<void()> cb_find_characteristics = buggerall;
		std::function<void()> cb_get_client_characteristic_configuration = buggerall;
		std::function<void()> cb_write_response = buggerall;
		std::function<void(Characteristic&, const PDUNotificationOrIndication&)> cb_notify_or_indicate;


		BLEGATTStateMachine(const std::string& addr);

		int socket();
		


		void read_primary_services();
		void find_all_characteristics();
		void get_client_characteristic_configuration();
		void read_and_process_next();
		void set_notify_and_indicate(Characteristic& c, bool notify, bool indicate);


		void do_standard_scan(std::function<void()>& cb);
};


void pretty_print_tree(const BLEGATTStateMachine& s);

#endif
