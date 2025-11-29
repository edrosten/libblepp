/*
 *
 *  blepp - Implementation of the Generic ATTribute Protocol
 *
 *  Copyright (C) 2024
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

#ifndef __INC_BLEPP_TRANSPORT_H
#define __INC_BLEPP_TRANSPORT_H

#include <blepp/blepp_config.h>

#ifdef BLEPP_SERVER_SUPPORT

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>
#include <blepp/blestatemachine.h>

namespace BLEPP
{
	/// Advertising parameters for BLE peripheral mode
	struct AdvertisingParams
	{
		std::string device_name;
		std::vector<UUID> service_uuids;
		uint16_t appearance = 0;

		/// Advertising interval in milliseconds (min)
		uint16_t min_interval_ms = 100;

		/// Advertising interval in milliseconds (max)
		uint16_t max_interval_ms = 200;

		/// Service data (UUID16 + data)
		uint16_t service_data_uuid16 = 0;
		std::vector<uint8_t> service_data;

		/// Raw advertising data (max 31 bytes)
		uint8_t advertising_data[31];
		uint8_t advertising_data_len = 0;

		/// Raw scan response data (max 31 bytes)
		uint8_t scan_response_data[31];
		uint8_t scan_response_data_len = 0;
	};

	/// Connection parameters
	struct ConnectionParams
	{
		uint16_t conn_handle;
		std::string peer_address;
		uint8_t peer_address_type;
		uint16_t mtu = 23;  // Default ATT MTU
	};

	/// Hardware abstraction layer for BLE transport
	/// This allows support for both standard BlueZ and Nimble-specific interfaces
	class BLETransport
	{
	public:
		virtual ~BLETransport() = default;

		/// Start advertising with the given parameters
		/// @return 0 on success, negative error code on failure
		virtual int start_advertising(const AdvertisingParams& params) = 0;

		/// Stop advertising
		/// @return 0 on success, negative error code on failure
		virtual int stop_advertising() = 0;

		/// Check if currently advertising
		virtual bool is_advertising() const = 0;

		/// Accept an incoming connection (blocking or use get_fd() for async)
		/// @return 0 on success, negative error code on failure
		virtual int accept_connection() = 0;

		/// Disconnect a connection
		/// @param conn_handle Connection handle to disconnect
		/// @return 0 on success, negative error code on failure
		virtual int disconnect(uint16_t conn_handle) = 0;

		/// Get file descriptor for select()/poll() integration
		/// @return File descriptor or -1 if not supported
		virtual int get_fd() const = 0;

		/// Send ATT PDU to a connected peer
		/// @param conn_handle Connection handle
		/// @param data PDU data
		/// @param len PDU length
		/// @return Number of bytes sent, or negative error code
		virtual int send_pdu(uint16_t conn_handle, const uint8_t* data, size_t len) = 0;

		/// Receive ATT PDU from a connected peer
		/// @param conn_handle Connection handle
		/// @param buf Buffer to receive data
		/// @param len Buffer size
		/// @return Number of bytes received, or negative error code
		virtual int recv_pdu(uint16_t conn_handle, uint8_t* buf, size_t len) = 0;

		/// Set MTU for a connection
		/// @param conn_handle Connection handle
		/// @param mtu MTU value (23-512)
		/// @return 0 on success, negative error code on failure
		virtual int set_mtu(uint16_t conn_handle, uint16_t mtu) = 0;

		/// Get current MTU for a connection
		/// @param conn_handle Connection handle
		/// @return MTU value
		virtual uint16_t get_mtu(uint16_t conn_handle) const = 0;

		/// Process pending events (non-blocking)
		/// This should be called from the event loop
		/// @return 0 on success, negative error code on failure
		virtual int process_events() = 0;

		// Callbacks

		/// Called when a client connects
		std::function<void(const ConnectionParams&)> on_connected;

		/// Called when a client disconnects
		std::function<void(uint16_t conn_handle)> on_disconnected;

		/// Called when ATT data is received
		std::function<void(uint16_t conn_handle, const uint8_t* data, size_t len)> on_data_received;

		/// Called when MTU changes
		std::function<void(uint16_t conn_handle, uint16_t mtu)> on_mtu_changed;
	};

	/// Factory function to create appropriate server transport based on build configuration
	/// Tries BlueZ first (if available), then Nimble
	/// @return Pointer to transport implementation (caller owns), nullptr if none available
	BLETransport* create_server_transport();

#ifdef BLEPP_BLUEZ_SUPPORT
	/// Create BlueZ server transport explicitly
	/// @return Pointer to BlueZ transport implementation (caller owns)
	BLETransport* create_bluez_server_transport();
#endif

#ifdef BLEPP_NIMBLE_SUPPORT
	/// Create Nimble server transport explicitly
	/// @return Pointer to Nimble transport implementation (caller owns)
	BLETransport* create_nimble_server_transport();
#endif

} // namespace BLEPP

#endif // BLEPP_SERVER_SUPPORT
#endif // __INC_BLEPP_TRANSPORT_H
