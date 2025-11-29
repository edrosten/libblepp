
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
#include <unistd.h>
#include <blepp/blestatemachine.h> //for UUID. FIXME mofo
#include <blepp/bleclienttransport.h>

#ifdef BLEPP_BLUEZ_SUPPORT
#include <bluetooth/hci.h>
#endif

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

		Name*  local_name = nullptr;
		Flags* flags = nullptr;

		std::vector<std::vector<uint8_t>> manufacturer_specific_data;
		std::vector<std::vector<uint8_t>> service_data;
		std::vector<std::vector<uint8_t>> unparsed_data_with_types;
		std::vector<std::vector<uint8_t>> raw_packet;

		AdvertisingResponse() = default;
		~AdvertisingResponse() {
			delete local_name;
			delete flags;
		}

		// Copy constructor
		AdvertisingResponse(const AdvertisingResponse& other)
			: address(other.address)
			, type(other.type)
			, rssi(other.rssi)
			, UUIDs(other.UUIDs)
			, uuid_16_bit_complete(other.uuid_16_bit_complete)
			, uuid_32_bit_complete(other.uuid_32_bit_complete)
			, uuid_128_bit_complete(other.uuid_128_bit_complete)
			, local_name(other.local_name ? new Name(*other.local_name) : nullptr)
			, flags(other.flags ? new Flags(*other.flags) : nullptr)
			, manufacturer_specific_data(other.manufacturer_specific_data)
			, service_data(other.service_data)
			, unparsed_data_with_types(other.unparsed_data_with_types)
			, raw_packet(other.raw_packet)
		{}

		// Copy assignment
		AdvertisingResponse& operator=(const AdvertisingResponse& other) {
			if (this != &other) {
				address = other.address;
				type = other.type;
				rssi = other.rssi;
				UUIDs = other.UUIDs;
				uuid_16_bit_complete = other.uuid_16_bit_complete;
				uuid_32_bit_complete = other.uuid_32_bit_complete;
				uuid_128_bit_complete = other.uuid_128_bit_complete;
				delete local_name;
				local_name = other.local_name ? new Name(*other.local_name) : nullptr;
				delete flags;
				flags = other.flags ? new Flags(*other.flags) : nullptr;
				manufacturer_specific_data = other.manufacturer_specific_data;
				service_data = other.service_data;
				unparsed_data_with_types = other.unparsed_data_with_types;
				raw_packet = other.raw_packet;
			}
			return *this;
		}

		// Move constructor
		AdvertisingResponse(AdvertisingResponse&& other) noexcept
			: address(std::move(other.address))
			, type(other.type)
			, rssi(other.rssi)
			, UUIDs(std::move(other.UUIDs))
			, uuid_16_bit_complete(other.uuid_16_bit_complete)
			, uuid_32_bit_complete(other.uuid_32_bit_complete)
			, uuid_128_bit_complete(other.uuid_128_bit_complete)
			, local_name(other.local_name)
			, flags(other.flags)
			, manufacturer_specific_data(std::move(other.manufacturer_specific_data))
			, service_data(std::move(other.service_data))
			, unparsed_data_with_types(std::move(other.unparsed_data_with_types))
			, raw_packet(std::move(other.raw_packet))
		{
			other.local_name = nullptr;
			other.flags = nullptr;
		}

		// Move assignment
		AdvertisingResponse& operator=(AdvertisingResponse&& other) noexcept {
			if (this != &other) {
				address = std::move(other.address);
				type = other.type;
				rssi = other.rssi;
				UUIDs = std::move(other.UUIDs);
				uuid_16_bit_complete = other.uuid_16_bit_complete;
				uuid_32_bit_complete = other.uuid_32_bit_complete;
				uuid_128_bit_complete = other.uuid_128_bit_complete;
				delete local_name;
				local_name = other.local_name;
				other.local_name = nullptr;
				delete flags;
				flags = other.flags;
				other.flags = nullptr;
				manufacturer_specific_data = std::move(other.manufacturer_specific_data);
				service_data = std::move(other.service_data);
				unparsed_data_with_types = std::move(other.unparsed_data_with_types);
				raw_packet = std::move(other.raw_packet);
			}
			return *this;
		}
	};

	// HCI/BLE Scanner error classes - available for all transports

	/// Generic HCI scanner error exception class
	class HCIScannerError: public std::runtime_error
	{
		public:
			HCIScannerError(const std::string& why);
	};

	/// HCI device spat out invalid data during parsing
	class HCIParseError: public HCIScannerError
	{
		public:
			using HCIScannerError::HCIScannerError;
	};

	/// Parse HCI advertising packet data
	/// This is a standalone function available regardless of transport backend
	/// It's also accessible via HCIScanner::parse_packet() for compatibility
	/// @param p Raw HCI packet data
	/// @return Vector of parsed advertising responses
	/// @throws HCIParseError if packet is malformed
	std::vector<AdvertisingResponse> parse_advertisement_packet(const std::vector<uint8_t>& p);

	// Forward declaration
	class BLEClientTransport;

	/// Transport-agnostic BLE Scanner class
	/// Works with any BLEClientTransport implementation (BlueZ, Nimble, etc.)
	/// This is the recommended scanner class for new code.
	class BLEScanner
	{
	public:
		using FilterDuplicates = ScanParams::FilterDuplicates;

		/// Constructor
		/// @param transport Pointer to transport implementation (must outlive scanner)
		explicit BLEScanner(BLEClientTransport* transport);

		~BLEScanner();

		/// Start scanning for BLE devices with custom parameters
		/// @param params Scan parameters (interval, window, type, etc.)
		void start(const ScanParams& params);

		/// Start scanning for BLE devices (backwards compatible)
		/// @param passive If true, use passive scanning (lower power)
		void start(bool passive = false);

		/// Stop scanning
		void stop();

		/// Get advertisements (blocking call)
		/// @param timeout_ms Timeout in milliseconds (0 = no timeout)
		/// @return Vector of advertising responses
		std::vector<AdvertisingResponse> get_advertisements(int timeout_ms = 0);

		/// Check if scanner is running
		bool is_running() const { return running_; }

	private:
		struct FilterEntry
		{
			explicit FilterEntry(const AdvertisingResponse&);
			const std::string mac_address;
			int type;
			bool operator<(const FilterEntry&) const;
		};

		BLEClientTransport* transport_;
		bool running_;
		FilterDuplicates filter_mode_;
		std::set<FilterEntry> scanned_devices_;
	};

}

#endif
