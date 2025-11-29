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

#ifndef __INC_BLEPP_BLECLIENTTRANSPORT_H
#define __INC_BLEPP_BLECLIENTTRANSPORT_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace BLEPP
{
	/// Scan parameters for BLE device discovery
	struct ScanParams
	{
		enum class ScanType : uint8_t {
			Passive = 0x00,  // Passive scanning (no scan requests)
			Active  = 0x01   // Active scanning (send scan requests)
		};

		enum class FilterPolicy : uint8_t {
			All           = 0x00,  // Accept all advertising packets
			WhitelistOnly = 0x01   // Accept only whitelisted devices
		};

		enum class FilterDuplicates : uint8_t {
			Off      = 0x00,  // No filtering - get all advertisement events
			Software = 0x01,  // Filter duplicates in software only
			Hardware = 0x02   // Filter duplicates in hardware only
		};

		ScanType scan_type = ScanType::Active;
		uint16_t interval_ms = 1280;    // Scan interval in ms (default: 1280ms for WiFi coexistence)
		uint16_t window_ms = 26;        // Scan window in ms (default: 2% duty cycle)
		FilterPolicy filter_policy = FilterPolicy::All;
		FilterDuplicates filter_duplicates = FilterDuplicates::Software;  // Duplicate filtering mode
	};

	/// Advertisement data received during scanning
	struct AdvertisementData
	{
		std::string address;
		uint8_t address_type;  // 0=public, 1=random
		int8_t rssi;
		uint8_t event_type;    // ADV_IND, SCAN_RSP, etc.
		std::vector<uint8_t> data;
	};

	/// Connection parameters for BLE client connections
	struct ClientConnectionParams
	{
		std::string peer_address;
		uint8_t peer_address_type = 0;  // 0=public, 1=random
		uint16_t min_interval = 24;     // Connection interval min (units of 1.25ms)
		uint16_t max_interval = 40;     // Connection interval max (units of 1.25ms)
		uint16_t latency = 0;           // Slave latency
		uint16_t timeout = 400;         // Supervision timeout (units of 10ms)
	};

	/// Abstract interface for BLE client transport layer
	/// Supports both BlueZ (HCI/L2CAP) and Nimble (ioctl) backends
	class BLEClientTransport
	{
	public:
		virtual ~BLEClientTransport() = default;

		// ===== Scanning Operations =====

		/// Start scanning for BLE devices
		/// @param params Scan parameters
		/// @return 0 on success, negative on error
		virtual int start_scan(const ScanParams& params) = 0;

		/// Stop scanning
		/// @return 0 on success, negative on error
		virtual int stop_scan() = 0;

		/// Get received advertisements (blocking or non-blocking based on implementation)
		/// @param ads Output vector to receive advertisements
		/// @param timeout_ms Timeout in milliseconds (0 = non-blocking, -1 = blocking)
		/// @return Number of advertisements received, negative on error
		virtual int get_advertisements(std::vector<AdvertisementData>& ads, int timeout_ms = 0) = 0;

		// ===== Connection Operations =====

		/// Connect to a BLE device
		/// @param params Connection parameters including peer address
		/// @return File descriptor/handle on success, negative on error
		virtual int connect(const ClientConnectionParams& params) = 0;

		/// Disconnect from device
		/// @param fd File descriptor/handle from connect()
		/// @return 0 on success, negative on error
		virtual int disconnect(int fd) = 0;

		/// Get the file descriptor for select/poll operations
		/// @param fd Connection file descriptor/handle
		/// @return File descriptor suitable for select/poll, or -1 if not applicable
		virtual int get_fd(int fd) const = 0;

		// ===== Data Transfer Operations =====

		/// Send ATT PDU to connected device
		/// @param fd Connection file descriptor/handle
		/// @param data PDU data to send
		/// @param len Length of data
		/// @return Number of bytes sent, negative on error
		virtual int send(int fd, const uint8_t* data, size_t len) = 0;

		/// Receive ATT PDU from connected device
		/// @param fd Connection file descriptor/handle
		/// @param data Buffer to receive data
		/// @param max_len Maximum buffer size
		/// @return Number of bytes received, negative on error
		virtual int receive(int fd, uint8_t* data, size_t max_len) = 0;

		// ===== MTU Operations =====

		/// Get current MTU for connection
		/// @param fd Connection file descriptor/handle
		/// @return MTU value, or 23 (default) on error
		virtual uint16_t get_mtu(int fd) const = 0;

		/// Set MTU for connection
		/// @param fd Connection file descriptor/handle
		/// @param mtu New MTU value
		/// @return 0 on success, negative on error
		virtual int set_mtu(int fd, uint16_t mtu) = 0;

		// ===== Transport Information =====

		/// Get transport type name (for debugging)
		/// @return Transport name ("BlueZ", "Nimble", etc.)
		virtual const char* get_transport_name() const = 0;

		/// Check if transport is available on this system
		/// @return true if transport can be used
		virtual bool is_available() const = 0;

		/// Get the Bluetooth MAC address used by this transport
		/// @return MAC address as string in format "XX:XX:XX:XX:XX:XX", or empty string on error
		virtual std::string get_mac_address() const = 0;

		// ===== Callbacks (optional, for async operation) =====

		std::function<void(const AdvertisementData&)> on_advertisement;
		std::function<void(int fd)> on_connected;
		std::function<void(int fd)> on_disconnected;
		std::function<void(int fd, const uint8_t* data, size_t len)> on_data_received;
	};

	/// Factory function to create appropriate transport based on build configuration
	/// Tries BlueZ first (if available), then Nimble
	/// @return Pointer to transport implementation (caller owns), nullptr if none available
	BLEClientTransport* create_client_transport();

#ifdef BLEPP_BLUEZ_SUPPORT
	/// Create BlueZ client transport explicitly
	/// @return Pointer to BlueZ transport implementation (caller owns)
	BLEClientTransport* create_bluez_client_transport();
#endif

#ifdef BLEPP_NIMBLE_SUPPORT
	/// Create Nimble client transport explicitly
	/// @return Pointer to Nimble transport implementation (caller owns)
	BLEClientTransport* create_nimble_client_transport();
#endif

} // namespace BLEPP

#endif // __INC_BLEPP_BLECLIENTTRANSPORT_H
