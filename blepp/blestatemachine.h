#ifndef __INC_LIBATTGATT_BLESTATEMACHINE_H
#define __INC_LIBATTGATT_BLESTATEMACHINE_H
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

#include <vector>
#include <stdexcept>
#include <functional>

#include <blepp/logging.h>
#include <blepp/bledevice.h>
#include <blepp/att_pdu.h>


#include <bluetooth/l2cap.h>

namespace BLEPP
{
	enum class WriteType
	{
		Request,
		Command
	};
	
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
				throw std::runtime_error("Invalid packet size in GATTReadCharacteristic");
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
				throw std::runtime_error("Invalid packet size in GATTReadCharacteristic");
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
		Disconnected,
		Connecting,
		Idle,
		ReadingPrimaryService,
		FindAllCharacteristics,
		GetClientCharaceristicConfiguration,
		AwaitingWriteResponse,
		AwaitingReadResponse,
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

		UUID(const std::string& uuid_str)
		{
			//FIXME: check errors!
			bt_string_to_uuid(this, uuid_str.c_str());
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

		void set_notify_and_indicate(bool , bool, WriteType type=WriteType::Request );
		std::function<void(const PDUNotificationOrIndication&)> cb_notify_or_indicate;
		std::function<void(const PDUReadResponse&)> cb_read;

		void write_request(const uint8_t* data, int length);
		void write_command(const uint8_t* data, int length);
		void read_request();

		// Shortcuts for writing values without explicitly using sizeof and pointer cast.
		// Don't forget to explicitly cast when writing numbers!, e.g. write_request((uint16_t)3)
		// Also, don't forget about alignment when sending structs. Suggest __attribute__((pack))
		template <typename T>
		void write_request(const T& data)
		{
			write_request((const uint8_t*)&data, sizeof(data));
		}
		template <typename T>
		void write_command(const T& data)
		{
			write_command((const uint8_t*)&data, sizeof(data));
		}

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

	struct StateMachineGoneBad: public std::runtime_error
	{
		StateMachineGoneBad(const std::string& err)
		:std::runtime_error(err)
		{
		}
	};


	struct ServiceInfo
	{
		std::string name, id;
		UUID uuid;
	};

	struct PrimaryService
	{
		uint16_t start_handle;
		uint16_t end_handle;
		UUID uuid;
		std::vector<Characteristic> characteristics;
	};


	const ServiceInfo* lookup_service_by_UUID(const UUID& uuid);

	class BLEGATTStateMachine
	{
		public:

			struct Disconnect
			{
				enum Reason{
					ConnectionFailed,
					UnexpectedError,
					UnexpectedResponse,
					WriteError,
					ReadError,
					ConnectionClosed,
				} reason;
				
				static constexpr int NoErrorCode=1; // Any positive value
				int error_code;

				Disconnect(Reason r, int e)
				:reason(r), error_code(e)
				{
				}
			};
			static const char* get_disconnect_string(Disconnect); 

		private:
			struct sockaddr_l2 addr;
			
			int sock = -1;


			static void buggerall();
			static void buggerall2(Disconnect);

			BLEDevice dev;
			

			States state = Disconnected;
			int next_handle_to_read=-1;
			uint16_t read_req_handle=-1;
			int last_request=-1;
			
			std::vector<std::uint8_t> buf;


			struct PrimaryServiceInfo
			{
				std::string name, id;
				UUID uuid;
			};

			void reset();
			void state_machine_write();
			void unexpected_error(const PDUErrorResponse&);
			void fail(Disconnect);
			Characteristic* characteristic_of_handle(uint16_t handle);
			void close_and_cleanup();

		public:


			std::vector<PrimaryService> primary_services;

			std::function<void()> cb_connected = buggerall;
			std::function<void(Disconnect)> cb_disconnected = buggerall2;
			std::function<void()> cb_services_read = buggerall;
			std::function<void()> cb_find_characteristics = buggerall;
			std::function<void()> cb_get_client_characteristic_configuration = buggerall;
			std::function<void()> cb_write_response = buggerall;
			std::function<void(Characteristic&, const PDUNotificationOrIndication&)> cb_notify_or_indicate;
			std::function<void(Characteristic&, const PDUReadResponse&)> cb_read;


			BLEGATTStateMachine();
			~BLEGATTStateMachine();

			
			void connect_blocking(const std::string& addres);
			void connect_nonblocking(const std::string& addres);
			void connect(const std::string& addresa, bool blocking, bool pubaddr = true, std::string device = "");
			void close();

			int socket();
		
			bool wait_on_write();
			
			bool is_idle()
			{
				return state == Idle;
			}
			
			void send_write_request(uint16_t handle, const uint8_t* data, int length);
			void send_write_command(uint16_t handle, const uint8_t* data, int length);
			void send_read_request(uint16_t handle);

			void read_primary_services();
			void find_all_characteristics();
			void get_client_characteristic_configuration();
			void read_and_process_next();
			void write_and_process_next();
			void set_notify_and_indicate(Characteristic& c, bool notify, bool indicate, WriteType type = WriteType::Request);


			void setup_standard_scan(std::function<void()>& cb);
	};


	void pretty_print_tree(const BLEGATTStateMachine& s);



	class SocketAllocationFailed: public std::runtime_error { using runtime_error::runtime_error; };
	class SocketBindFailed: public std::runtime_error { using runtime_error::runtime_error; };
	class SocketGetSockOptFailed: public std::runtime_error { using runtime_error::runtime_error; };
	class SocketConnectFailed: public std::runtime_error { using runtime_error::runtime_error; };
}
#endif
