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

#ifndef __INC_BLEPP_NIMBLE_CLIENT_TRANSPORT_H
#define __INC_BLEPP_NIMBLE_CLIENT_TRANSPORT_H

#include <blepp/blepp_config.h>

#ifdef BLEPP_NIMBLE_SUPPORT

#include <blepp/bleclienttransport.h>
#include <mutex>
#include <map>
#include <queue>
#include <set>
#include <vector>
#include <thread>
#include <atomic>

// Forward declare Nimble structures
struct ble_gap_event;
struct ble_gap_disc_desc;
struct ble_gatt_access_ctxt;

namespace BLEPP
{
	/// Nimble-based client transport using Apache Nimble BLE stack
	class NimbleClientTransport : public BLEClientTransport
	{
	public:
		NimbleClientTransport();
		virtual ~NimbleClientTransport();

		// Scanning operations
		int start_scan(const ScanParams& params) override;
		int stop_scan() override;
		int get_advertisements(std::vector<AdvertisementData>& ads, int timeout_ms = 0) override;

		// Connection operations
		int connect(const ClientConnectionParams& params) override;
		int disconnect(int fd) override;
		int get_fd(int fd) const override;

		// Data transfer
		int send(int fd, const uint8_t* data, size_t len) override;
		int receive(int fd, uint8_t* data, size_t max_len) override;

		// MTU operations
		uint16_t get_mtu(int fd) const override;
		int set_mtu(int fd, uint16_t mtu) override;

		// Transport information
		const char* get_transport_name() const override { return "Nimble"; }
		bool is_available() const override;
		std::string get_mac_address() const override;

		// Nimble callbacks (static wrappers that call instance methods)
		static int gap_event_callback(struct ble_gap_event* event, void* arg);
		static int gatt_event_callback(uint16_t conn_handle, uint16_t attr_handle,
		                               struct ble_gatt_access_ctxt* ctxt, void* arg);

		// Public for static callback access
		std::atomic<bool> synchronized_;  // True when Nimble host has synchronized

	private:
		struct ConnectionInfo {
			uint16_t conn_handle;  // Nimble connection handle
			uint16_t mtu;
			std::string address;
			std::queue<std::vector<uint8_t>> rx_queue;  // Received data queue
			bool connected;
		};

		bool initialized_;
		bool scanning_;

		ScanParams scan_params_;
		std::queue<AdvertisementData> scan_results_;
		std::mutex scan_mutex_;
		std::set<std::string> seen_devices_;  // For duplicate filtering

		std::map<int, ConnectionInfo> connections_;  // fd -> connection info
		std::map<uint16_t, int> handle_to_fd_;       // Nimble handle -> fd
		mutable std::mutex conn_mutex_;  // mutable so get_mtu() can lock in const context

		int next_fd_;  // For generating fake file descriptors
		mutable std::string mac_address_;  // Cached BLE MAC address (mutable for lazy init in const getter)

		// Nimble initialization
		int initialize_nimble();
		void shutdown_nimble();

		// Instance GAP event handlers
		int handle_gap_event(struct ble_gap_event* event);
		void handle_disc_event(const struct ble_gap_disc_desc* disc);
		void handle_connect_event(struct ble_gap_event* event);
		void handle_disconnect_event(struct ble_gap_event* event);
		void handle_mtu_event(struct ble_gap_event* event);
		void handle_notify_rx_event(struct ble_gap_event* event);

		// Helper functions
		int allocate_fd();
		void release_fd(int fd);
		std::string addr_to_string(const uint8_t addr[6]);
		void string_to_addr(const std::string& str, uint8_t addr[6]);
	};

} // namespace BLEPP

#endif // BLEPP_NIMBLE_SUPPORT

#endif // __INC_BLEPP_NIMBLE_CLIENT_TRANSPORT_H
