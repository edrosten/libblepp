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

#ifndef __INC_BLEPP_BLUEZ_TRANSPORT_H
#define __INC_BLEPP_BLUEZ_TRANSPORT_H

#include <blepp/blepp_config.h>

#ifdef BLEPP_SERVER_SUPPORT

#include <blepp/bletransport.h>
#include <map>
#include <memory>

namespace BLEPP
{
	/// BlueZ-based BLE transport implementation
	/// Uses standard Linux Bluetooth stack (HCI for advertising, L2CAP for data)
	class BlueZTransport : public BLETransport
	{
	public:
		/// Constructor
		/// @param hci_dev_id HCI device ID (default: 0 for hci0)
		explicit BlueZTransport(int hci_dev_id = 0);

		/// Destructor - cleans up sockets and stops advertising
		~BlueZTransport() override;

		// BLETransport interface implementation

		int start_advertising(const AdvertisingParams& params) override;
		int stop_advertising() override;
		bool is_advertising() const override;

		int accept_connection() override;
		int disconnect(uint16_t conn_handle) override;
		int get_fd() const override;

		int send_pdu(uint16_t conn_handle, const uint8_t* data, size_t len) override;
		int recv_pdu(uint16_t conn_handle, uint8_t* buf, size_t len) override;

		int set_mtu(uint16_t conn_handle, uint16_t mtu) override;
		uint16_t get_mtu(uint16_t conn_handle) const override;

		int process_events() override;

	private:
		struct Connection
		{
			int fd;
			uint16_t conn_handle;
			std::string peer_addr;
			uint16_t mtu;
		};

		int hci_dev_id_;
		int hci_fd_;                // HCI socket for advertising control
		int l2cap_listen_fd_;       // L2CAP listening socket (CID 4 - ATT)
		bool advertising_;
		uint16_t next_conn_handle_;

		std::map<uint16_t, Connection> connections_;

		// Helper methods

		/// Send SSV6158 vendor command to enable ACL/Event routing
		int send_ssv_acl_routing_command(int fd);

		/// Open HCI device socket
		int open_hci_device();

		/// Set up L2CAP server socket
		int setup_l2cap_server();

		/// Configure advertising parameters via HCI
		int set_advertising_parameters(const AdvertisingParams& params);

		/// Set advertising data via HCI
		int set_advertising_data(const AdvertisingParams& params);

		/// Set scan response data via HCI
		int set_scan_response_data(const AdvertisingParams& params);

		/// Enable/disable advertising via HCI
		int set_advertising_enable(bool enable);

		/// Build advertising data from parameters
		int build_advertising_data(const AdvertisingParams& params,
		                          uint8_t* data, uint8_t* len);

		/// Build scan response data from parameters
		int build_scan_response_data(const AdvertisingParams& params,
		                            uint8_t* data, uint8_t* len);

		/// Accept connection on L2CAP socket
		int accept_l2cap_connection();

		/// Close all connections and sockets
		void cleanup();
	};

} // namespace BLEPP

#endif // BLEPP_SERVER_SUPPORT
#endif // __INC_BLEPP_BLUEZ_TRANSPORT_H
