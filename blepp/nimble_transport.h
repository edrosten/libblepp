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

#ifndef __INC_BLEPP_NIMBLE_TRANSPORT_H
#define __INC_BLEPP_NIMBLE_TRANSPORT_H

#include <blepp/blepp_config.h>

#ifdef BLEPP_NIMBLE_SUPPORT

#include <blepp/bletransport.h>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <semaphore.h>
#include <vector>

namespace BLEPP
{
	// Forward declarations
	struct GATTServiceDef;
	struct GATTCharacteristicDef;
	struct GATTDescriptorDef;
	/// Nimble-based BLE transport implementation
	/// Uses Nimble's /dev/atbm_ioctl interface for communication with BLE controller
	class NimbleTransport : public BLETransport
	{
	public:
		/// Constructor
		/// @param device_path Path to Nimble ioctl device (default: /dev/atbm_ioctl)
		explicit NimbleTransport(const char* device_path = "/dev/atbm_ioctl");

		/// Destructor - cleans up and stops event thread
		~NimbleTransport() override;

		// BLETransport interface implementation

		int start_advertising(const AdvertisingParams& params) override;
		int stop_advertising() override;
		bool is_advertising() const override;

		int accept_connection() override;
		int disconnect(uint16_t conn_handle) override;
		int get_fd() const override { return ioctl_fd_; }

		int send_pdu(uint16_t conn_handle, const uint8_t* data, size_t len) override;
		int recv_pdu(uint16_t conn_handle, uint8_t* buf, size_t len) override;

		int set_mtu(uint16_t conn_handle, uint16_t mtu) override;
		uint16_t get_mtu(uint16_t conn_handle) const override;

		int process_events() override;

		/// Register GATT services with NimBLE stack
		/// @param services Vector of service definitions
		/// @return 0 on success, negative on error
		int register_services(const std::vector<struct GATTServiceDef>& services);

		/// Called from signal handler to notify event thread
		void signal_event();

		/// Convert and register services with NimBLE GATTS
		/// Called from sync callback after stack initialization
		int convert_and_register_services();

		/// Restart advertising with last used parameters
		/// Called from GAP event callback after disconnect
		void restart_advertising();

	private:
		struct Connection
		{
			uint16_t conn_handle;
			std::string peer_addr;
			uint16_t mtu;
		};

		struct PendingPDU
		{
			uint16_t conn_handle;
			std::vector<uint8_t> data;
		};

		std::string device_path_;
		int ioctl_fd_;
		bool advertising_;
		uint16_t next_conn_handle_;
		bool host_task_started_;  // Track if NimBLE host task has been started

		mutable std::mutex connections_mutex_;
		std::map<uint16_t, Connection> connections_;

		// Event loop thread
		std::thread event_thread_;
		std::atomic<bool> running_;

		// Semaphores for event synchronization
		sem_t event_sem_;   // Posted by signal handler, waited by event thread
		sem_t ioctl_sem_;   // Protects ioctl operations

		// Received PDU queue
		std::mutex rx_mutex_;
		std::queue<PendingPDU> rx_queue_;

		// Internal structures for Nimble ioctl communication
		struct status_async {
			uint8_t type;           // Event type
			uint8_t driver_mode;    // Sub-type or reason
			uint8_t list_empty;     // 1 if no more events, 0 if more pending
			uint8_t event_buffer[512];
		};

		struct wsm_hdr {
			uint16_t len;   // Length of data following this header
			uint16_t id;    // Message type ID
		};

		// GATT service bridge
		std::vector<struct GATTServiceDef> service_defs_;  // Store original service definitions

		// Advertising parameters (stored for restart after disconnect)
		AdvertisingParams adv_params_;
		bool adv_params_valid_;

		// Helper methods

		/// Event loop thread function
		void event_loop_thread();

		/// Process Nimble event structure
		void process_nimble_event(const struct status_async* event);

		/// Process HCI event data
		void process_hci_event(const uint8_t* data, size_t len);

		/// Send HCI command
		int send_hci_command(const uint8_t* cmd, size_t len);

		/// Cleanup resources
		void cleanup();
	};

} // namespace BLEPP

#endif // BLEPP_NIMBLE_SUPPORT
#endif // __INC_BLEPP_NIMBLE_TRANSPORT_H
