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

#ifndef __INC_BLEPP_BLUEZ_CLIENT_TRANSPORT_H
#define __INC_BLEPP_BLUEZ_CLIENT_TRANSPORT_H

#include <blepp/blepp_config.h>

#ifdef BLEPP_BLUEZ_SUPPORT

#include <blepp/bleclienttransport.h>
#include <map>
#include <set>

namespace BLEPP
{
	/// BlueZ-based client transport using HCI for scanning and L2CAP for connections
	class BlueZClientTransport : public BLEClientTransport
	{
	public:
		BlueZClientTransport();
		virtual ~BlueZClientTransport();

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
		const char* get_transport_name() const override { return "BlueZ"; }
		bool is_available() const override;
		std::string get_mac_address() const override;

	private:
		struct ConnectionInfo {
			int fd;
			uint16_t mtu;
			std::string address;
		};

		int hci_dev_id_;
		int hci_fd_;                    // HCI device for scanning
		bool scanning_;
		ScanParams scan_params_;

		std::set<std::string> seen_devices_;  // For duplicate filtering
		std::map<int, ConnectionInfo> connections_;
		mutable std::string mac_address_;  // Cached BLE MAC address

		int open_hci_device();
		void close_hci_device();
		int set_scan_parameters(const ScanParams& params);
		int set_scan_enable(bool enable, bool filter_duplicates);
		int read_hci_events(std::vector<AdvertisementData>& ads, int timeout_ms);
		int parse_advertising_report(const uint8_t* data, size_t len, std::vector<AdvertisementData>& ads);
	};

} // namespace BLEPP

#endif // BLEPP_BLUEZ_SUPPORT

#endif // __INC_BLEPP_BLUEZ_CLIENT_TRANSPORT_H
