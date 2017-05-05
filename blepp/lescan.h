
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

#ifndef __INC_BLEPP_LESCAN_H
#define __INC_BLEPP_LESCAN_H

#include <stdexcept>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <set>
#include <boost/optional.hpp>
#include <blepp/blestatemachine.h> //for UUID. FIXME mofo
#include <bluetooth/hci.h>

namespace BLEPP
{
	enum class LeAdvertisingEventType
	{	
		ADV_IND = 0x00, //Connectable undirected advertising 
						//Broadcast; any device can connect or ask for more information
		ADV_DIRECT_IND = 0x01, //Connectable Directed
							   //Targeted; a single known device that can only connect
		ADV_SCAN_IND = 0x02, //Scannable Undirected
							 //Purely informative broadcast; devices can ask for more information
		ADV_NONCONN_IND = 0x03, //Non-Connectable Undirected
								//Purely informative broadcast; no device can connect or even ask for more information
		SCAN_RSP = 0x04, //Result coming back after a scan request
	};

	//Is this the best design. I'm not especially convinced.
	//It seems pretty wretched.
	struct AdvertisingResponse
	{
		std::string address;
		LeAdvertisingEventType type;
		int8_t rssi;
		struct Name
		{
			std::string name;
			bool complete;
		};

		struct Flags
		{
			bool LE_limited_discoverable=0;
			bool LE_general_discoverable=0;
			bool BR_EDR_unsupported=0;
			bool simultaneous_LE_BR_controller=0;
			bool simultaneous_LE_BR_host=0;

			std::vector<uint8_t> flag_data;
			Flags(std::vector<uint8_t>&&);
		};

		std::vector<UUID> UUIDs;
		bool uuid_16_bit_complete=0;
		bool uuid_32_bit_complete=0;
		bool uuid_128_bit_complete=0;
		
		boost::optional<Name>  local_name;
		boost::optional<Flags> flags;

		std::vector<std::vector<uint8_t>> manufacturer_specific_data;
		std::vector<std::vector<uint8_t>> service_data;
		std::vector<std::vector<uint8_t>> unparsed_data_with_types;

	};

	/// Class for scanning for BLE devices
	/// this must be run as root, because it requires getting packets from the HCI.
	/// The HCI requires root since it has no permissions on setting filters, so 
	/// anyone with an open HCI device can sniff all data.
	class HCIScanner
	{
		class FD
		{
			private:
				int fd=-1;

			public:
				operator int () const
				{
					return fd;
				}
				FD(int i)
				:fd(i)
				{
				}
				
				FD()=default;
				void set(int i)
				{
					fd = i;
				}

				~FD()
				{
					if(fd != -1)
						close(fd);
				}
		};


		public:
		
		enum class ScanType
		{
			Passive  = 0x00,
			Active   = 0x01,
		};

		enum class FilterDuplicates
		{
			Off, //Get all events
			Hardware, //Rely on hardware filtering only. Lower power draw, but can actually send
			          //duplicates if the device's builtin list gets overwhelmed.
			Software, //Get all events from the device and filter them by hand.
			Both      //The best and worst of both worlds. 
		};

		///Generic error exception class
		class Error: public std::runtime_error
		{
			public:
				Error(const std::string& why);
		};

		///Thrown only if a read() is interrupted. Only bother 
		///handling if you've got a non terminating exception handler
		class Interrupted: public Error
		{
			using Error::Error;
		};
		

		///IO error of some sort. Probably fatal for any bluetooth
		///based system. Or might be that the dongle was unplugged.
		class IOError: public Error
		{
			public:
			IOError(const std::string& why, int errno_val);
		};
		
		///HCI device spat out invalid data.
		///This is not good. Almost certainly fatal.
		class HCIError: public Error
		{
			using Error::Error;
		};

		HCIScanner();
		HCIScanner(bool start);
		HCIScanner(bool start, FilterDuplicates duplicates, ScanType, std::string device="");


		void start();
		void stop();
		
		///get the file descriptor.
		///Use with select(), poll() or whatever.
		int get_fd() const;
		
		~HCIScanner();

		///Blocking call. Use select() on the FD if you don't want to block.
		///This reads and parses the HCI packets.
		std::vector<AdvertisingResponse> get_advertisements();
		
		///Parse an HCI advertising packet. There's probably not much
		///reason to call this yourself.
		static std::vector<AdvertisingResponse> parse_packet(const std::vector<uint8_t>& p);

		private:
			struct FilterEntry
			{
				explicit FilterEntry(const AdvertisingResponse&);
				const std::string mac_address;
				int type;
				bool operator<(const FilterEntry&) const;
			};

			bool hardware_filtering;
			bool software_filtering;
			ScanType scan_type;

			FD hci_fd;
			bool running=0;
			hci_filter old_filter;
			
			///Read the HCI data, but don't parse it.
			std::vector<uint8_t> read_with_retry();
			std::set<FilterEntry> scanned_devices;
	};
}

#endif
