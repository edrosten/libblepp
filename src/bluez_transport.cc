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

#include <blepp/blepp_config.h>

#ifdef BLEPP_SERVER_SUPPORT

#include <blepp/bluez_transport.h>
#include <blepp/logging.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace BLEPP
{

BlueZTransport::BlueZTransport(int hci_dev_id)
	: hci_dev_id_(hci_dev_id)
	, hci_fd_(-1)
	, l2cap_listen_fd_(-1)
	, advertising_(false)
	, next_conn_handle_(1)
{
	ENTER();

	// Open HCI device
	hci_fd_ = open_hci_device();
	if (hci_fd_ < 0) {
		throw std::runtime_error("Failed to open HCI device");
	}

	// Set up L2CAP server
	if (setup_l2cap_server() < 0) {
		close(hci_fd_);
		throw std::runtime_error("Failed to set up L2CAP server");
	}

	LOG(Info, "BlueZTransport initialized on hci" << hci_dev_id_);
}

BlueZTransport::~BlueZTransport()
{
	ENTER();
	cleanup();
}

int BlueZTransport::send_ssv_acl_routing_command(int fd)
{
	ENTER();

	LOG(Info, "Sending SSV6158 ACL routing command (0xFC06)...");

	// SSV6158 vendor-specific HCI command: 0xFC06
	// This command tells the SSV6158 firmware to route ACL data and events
	// to/from the external host instead of handling them internally.
	// Without this command, the firmware accepts outgoing ACL packets but
	// doesn't actually transmit them over the BLE radio.
	//
	// Command structure:
	// OGF: 0x3F (vendor-specific)
	// OCF: 0x06 (SSV_ACL_EVT_TO_EXTERNAL_HOST)
	// Parameters: none

	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = 0x3F;  // OGF_VENDOR_CMD
	rq.ocf = 0x06;  // SSV_ACL_EVT_TO_EXTERNAL_HOST
	rq.cparam = nullptr;
	rq.clen = 0;
	rq.rparam = nullptr;
	rq.rlen = 0;

	int ret = hci_send_req(fd, &rq, 1000);  // 1 second timeout
	if (ret < 0) {
		LOG(Warning, "Failed to send SSV ACL routing command: " << strerror(errno));
		return -1;
	}

	LOG(Info, "SSV6158 ACL routing command sent successfully");

	// Give firmware a moment to process the command
	usleep(100000);  // 100ms delay

	return 0;
}

int BlueZTransport::open_hci_device()
{
	ENTER();

	// Get device ID if not specified
	int dev_id = hci_dev_id_;
	if (dev_id < 0) {
		dev_id = hci_get_route(nullptr);
		if (dev_id < 0) {
			LOG(Error, "No Bluetooth adapter found");
			return -1;
		}
	}

	// Open HCI socket
	int fd = hci_open_dev(dev_id);
	if (fd < 0) {
		LOG(Error, "Failed to open HCI device hci" << dev_id << ": " << strerror(errno));
		return -1;
	}

	LOG(Info, "Opened HCI device hci" << dev_id << " (fd=" << fd << ")");

	// Send SSV6158 vendor command to enable ACL/Event routing to external host
	// This is required for SSV6158 firmware to actually transmit ACL data packets
	// over the air when operating as a BLE peripheral/server.
	// Command: 0xFC06 (OGF=0x3F vendor-specific, OCF=0x06)
	LOG(Info, "About to call send_ssv_acl_routing_command...");
	int ssv_result = send_ssv_acl_routing_command(fd);
	LOG(Info, "send_ssv_acl_routing_command returned: " << ssv_result);
	if (ssv_result < 0) {
		LOG(Warning, "Failed to send SSV ACL routing command (not SSV6158 chip?)");
		// Don't fail initialization - this is only needed for SSV6158
	}

	return fd;
}

int BlueZTransport::setup_l2cap_server()
{
	ENTER();

	// Create L2CAP socket
	l2cap_listen_fd_ = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (l2cap_listen_fd_ < 0) {
		LOG(Error, "Failed to create L2CAP socket: " << strerror(errno));
		return -1;
	}

	// Bind to ATT CID (Channel ID 4)
	struct sockaddr_l2 addr = {};
	addr.l2_family = AF_BLUETOOTH;
	bdaddr_t any_addr = {{0, 0, 0, 0, 0, 0}};
	bacpy(&addr.l2_bdaddr, &any_addr);
	addr.l2_cid = htobs(4);  // ATT protocol CID
	addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;

	if (bind(l2cap_listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		LOG(Error, "Failed to bind L2CAP socket: " << strerror(errno));
		close(l2cap_listen_fd_);
		l2cap_listen_fd_ = -1;
		return -1;
	}

	// Listen for connections
	if (listen(l2cap_listen_fd_, 5) < 0) {
		LOG(Error, "Failed to listen on L2CAP socket: " << strerror(errno));
		close(l2cap_listen_fd_);
		l2cap_listen_fd_ = -1;
		return -1;
	}

	// Set non-blocking
	int flags = fcntl(l2cap_listen_fd_, F_GETFL, 0);
	fcntl(l2cap_listen_fd_, F_SETFL, flags | O_NONBLOCK);

	LOG(Info, "L2CAP server listening on CID 4 (fd=" << l2cap_listen_fd_ << ")");
	return 0;
}

int BlueZTransport::start_advertising(const AdvertisingParams& params)
{
	ENTER();

	if (advertising_) {
		LOG(Warning, "Already advertising");
		return 0;
	}

	// Set advertising parameters
	if (set_advertising_parameters(params) < 0) {
		LOG(Error, "Failed to set advertising parameters");
		return -1;
	}

	// Set advertising data
	if (set_advertising_data(params) < 0) {
		LOG(Error, "Failed to set advertising data");
		return -1;
	}

	// Set scan response data
	if (set_scan_response_data(params) < 0) {
		LOG(Error, "Failed to set scan response data");
		return -1;
	}

	// Enable advertising
	if (set_advertising_enable(true) < 0) {
		LOG(Error, "Failed to enable advertising");
		return -1;
	}

	advertising_ = true;
	LOG(Info, "Advertising started: " << params.device_name);
	return 0;
}

int BlueZTransport::stop_advertising()
{
	ENTER();

	if (!advertising_) {
		return 0;
	}

	if (set_advertising_enable(false) < 0) {
		LOG(Error, "Failed to disable advertising");
		return -1;
	}

	advertising_ = false;
	LOG(Info, "Advertising stopped");
	return 0;
}

bool BlueZTransport::is_advertising() const
{
	return advertising_;
}

int BlueZTransport::set_advertising_parameters(const AdvertisingParams& params)
{
	ENTER();

	// HCI LE Set Advertising Parameters command
	le_set_advertising_parameters_cp cmd;
	cmd.min_interval = htobs(params.min_interval_ms * 1000 / 625);  // Convert ms to 0.625ms units
	cmd.max_interval = htobs(params.max_interval_ms * 1000 / 625);
	cmd.advtype = 0x00;  // ADV_IND - connectable undirected advertising
	cmd.own_bdaddr_type = LE_PUBLIC_ADDRESS;
	cmd.direct_bdaddr_type = LE_PUBLIC_ADDRESS;
	memset(&cmd.direct_bdaddr, 0, sizeof(cmd.direct_bdaddr));
	cmd.chan_map = 0x07;  // All channels
	cmd.filter = 0x00;  // No filter policy

	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
	rq.cparam = &cmd;
	rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
	rq.rparam = nullptr;
	rq.rlen = 0;

	int ret = hci_send_req(hci_fd_, &rq, 1000);
	if (ret < 0) {
		LOG(Error, "hci_le_set_advertising_parameters failed: " << strerror(errno));
		return -1;
	}

	LOG(Debug, "Set advertising interval: " << params.min_interval_ms << "-" << params.max_interval_ms << "ms");
	return 0;
}

int BlueZTransport::build_advertising_data(const AdvertisingParams& params,
                                          uint8_t* data, uint8_t* len)
{
	ENTER();

	uint8_t* ptr = data;

	// If custom advertising data is provided, use it
	if (params.advertising_data_len > 0) {
		memcpy(data, params.advertising_data, params.advertising_data_len);
		*len = params.advertising_data_len;
		return 0;
	}

	// Otherwise build standard advertising data

	// Flags
	*ptr++ = 0x02;  // Length
	*ptr++ = 0x01;  // Type: Flags
	*ptr++ = 0x06;  // LE General Discoverable, BR/EDR not supported

	// Complete list of 16-bit Service UUIDs (if any)
	// Note: 128-bit UUIDs are placed in scan response data due to size constraints
	if (!params.service_uuids.empty()) {
		uint8_t* len_ptr = ptr++;
		*ptr++ = 0x03;  // Type: Complete list of 16-bit UUIDs

		uint8_t uuid_count = 0;
		for (const auto& uuid : params.service_uuids) {
			if (uuid.type == BT_UUID16) {
				*ptr++ = uuid.value.u16 & 0xFF;
				*ptr++ = (uuid.value.u16 >> 8) & 0xFF;
				uuid_count++;
			}
		}

		// Only keep this section if we actually added UUIDs
		if (uuid_count > 0) {
			*len_ptr = 1 + uuid_count * 2;  // Type byte + UUIDs
		} else {
			// No 16-bit UUIDs, revert the pointer
			ptr = len_ptr;
		}
	}

	// Device name
	if (!params.device_name.empty()) {
		size_t name_len = params.device_name.length();
		if (name_len > 29 - (ptr - data)) {
			name_len = 29 - (ptr - data);  // Truncate if needed
		}

		*ptr++ = 1 + name_len;  // Length
		*ptr++ = 0x09;  // Type: Complete local name
		memcpy(ptr, params.device_name.c_str(), name_len);
		ptr += name_len;
	}

	*len = ptr - data;
	LOG(Debug, "Built advertising data: " << (int)*len << " bytes");
	return 0;
}

int BlueZTransport::set_advertising_data(const AdvertisingParams& params)
{
	ENTER();

	uint8_t data[31];
	uint8_t len;

	if (build_advertising_data(params, data, &len) < 0) {
		return -1;
	}

	// HCI LE Set Advertising Data command
	le_set_advertising_data_cp cmd;
	cmd.length = len;
	memset(cmd.data, 0, sizeof(cmd.data));
	memcpy(cmd.data, data, len);

	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
	rq.cparam = &cmd;
	rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
	rq.rparam = nullptr;
	rq.rlen = 0;

	int ret = hci_send_req(hci_fd_, &rq, 1000);
	if (ret < 0) {
		LOG(Error, "hci_le_set_advertising_data failed: " << strerror(errno));
		return -1;
	}

	return 0;
}

int BlueZTransport::build_scan_response_data(const AdvertisingParams& params,
                                             uint8_t* data, uint8_t* len)
{
	ENTER();

	uint8_t* ptr = data;

	// If custom scan response data is provided, use it
	if (params.scan_response_data_len > 0) {
		memcpy(data, params.scan_response_data, params.scan_response_data_len);
		*len = params.scan_response_data_len;
		return 0;
	}

	// Otherwise, build scan response with 128-bit UUIDs if present
	std::vector<UUID> uuid128_list;
	for (const auto& uuid : params.service_uuids) {
		if (uuid.type == BT_UUID128) {
			uuid128_list.push_back(uuid);
		}
	}

	if (!uuid128_list.empty()) {
		// Add 128-bit UUIDs to scan response
		size_t space_needed = 2 + uuid128_list.size() * 16;  // Length + Type + UUIDs
		if (space_needed <= 31) {
			uint8_t* len_ptr = ptr++;
			*ptr++ = 0x07;  // Type: Complete list of 128-bit UUIDs

			for (const auto& uuid : uuid128_list) {
				// 128-bit UUIDs in advertising are in little-endian (different from ATT!)
				// So we use the data as-is
				memcpy(ptr, uuid.value.u128.data, 16);
				ptr += 16;
			}

			*len_ptr = 1 + uuid128_list.size() * 16;  // Type byte + UUIDs
		} else {
			LOG(Warning, "Not enough space for 128-bit UUIDs in scan response");
		}
	}

	*len = ptr - data;
	LOG(Debug, "Built scan response data: " << (int)*len << " bytes");
	return 0;
}

int BlueZTransport::set_scan_response_data(const AdvertisingParams& params)
{
	ENTER();

	uint8_t data[31];
	uint8_t len;

	if (build_scan_response_data(params, data, &len) < 0) {
		return -1;
	}

	// Only send scan response if there's data
	if (len > 0) {
		le_set_scan_response_data_cp cmd;
		cmd.length = len;
		memset(cmd.data, 0, sizeof(cmd.data));
		memcpy(cmd.data, data, len);

		struct hci_request rq;
		memset(&rq, 0, sizeof(rq));
		rq.ogf = OGF_LE_CTL;
		rq.ocf = OCF_LE_SET_SCAN_RESPONSE_DATA;
		rq.cparam = &cmd;
		rq.clen = LE_SET_SCAN_RESPONSE_DATA_CP_SIZE;
		rq.rparam = nullptr;
		rq.rlen = 0;

		int ret = hci_send_req(hci_fd_, &rq, 1000);
		if (ret < 0) {
			LOG(Error, "hci_le_set_scan_response_data failed: " << strerror(errno));
			return -1;
		}
	}

	return 0;
}

int BlueZTransport::set_advertising_enable(bool enable)
{
	ENTER();

	int ret = hci_le_set_advertise_enable(hci_fd_, enable ? 1 : 0, 1000);
	if (ret < 0) {
		LOG(Error, "hci_le_set_advertise_enable failed: " << strerror(errno));
		return -1;
	}

	LOG(Debug, "Advertising " << (enable ? "enabled" : "disabled"));
	return 0;
}

int BlueZTransport::accept_connection()
{
	ENTER();
	return accept_l2cap_connection();
}

int BlueZTransport::accept_l2cap_connection()
{
	ENTER();

	struct sockaddr_l2 addr = {};
	socklen_t addr_len = sizeof(addr);

	int client_fd = accept(l2cap_listen_fd_, (struct sockaddr*)&addr, &addr_len);
	if (client_fd < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;  // No connection pending
		}
		LOG(Error, "accept() failed: " << strerror(errno));
		return -1;
	}

	// Get peer address
	char peer_addr[18];
	ba2str(&addr.l2_bdaddr, peer_addr);

	// Create connection entry
	uint16_t conn_handle = next_conn_handle_++;
	Connection conn = {
		.fd = client_fd,
		.conn_handle = conn_handle,
		.peer_addr = peer_addr,
		.mtu = 23  // Default ATT MTU
	};

	connections_[conn_handle] = conn;

	LOG(Info, "Client connected: " << peer_addr << " (handle=" << conn_handle << ")");

	// Notify callback
	if (on_connected) {
		ConnectionParams params;
		params.conn_handle = conn_handle;
		params.peer_address = peer_addr;
		params.peer_address_type = addr.l2_bdaddr_type;
		params.mtu = 23;
		on_connected(params);
	}

	return 0;
}

int BlueZTransport::disconnect(uint16_t conn_handle)
{
	ENTER();

	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		LOG(Warning, "Connection handle " << conn_handle << " not found");
		return -1;
	}

	close(it->second.fd);
	connections_.erase(it);

	LOG(Info, "Disconnected connection handle " << conn_handle);

	if (on_disconnected) {
		on_disconnected(conn_handle);
	}

	return 0;
}

int BlueZTransport::get_fd() const
{
	return l2cap_listen_fd_;
}

int BlueZTransport::send_pdu(uint16_t conn_handle, const uint8_t* data, size_t len)
{
	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		LOG(Error, "Connection handle " << conn_handle << " not found");
		return -1;
	}

	// Log hex dump of data being sent
	std::stringstream hex_dump;
	hex_dump << std::hex << std::setfill('0');
	for (size_t i = 0; i < len; i++) {
		hex_dump << std::setw(2) << (int)data[i] << " ";
	}
	LOG(Debug, "Sending " << std::dec << len << " bytes: " << hex_dump.str());

	ssize_t sent = send(it->second.fd, data, len, 0);
	if (sent < 0) {
		LOG(Error, "send() failed: " << strerror(errno));
		return -1;
	}

	if ((size_t)sent != len) {
		LOG(Warning, "Partial send: sent=" << sent << " expected=" << len);
	} else {
		LOG(Debug, "Successfully sent " << sent << " bytes to connection " << conn_handle);
	}

	return sent;
}

int BlueZTransport::recv_pdu(uint16_t conn_handle, uint8_t* buf, size_t len)
{
	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		LOG(Error, "Connection handle " << conn_handle << " not found");
		return -1;
	}

	ssize_t received = recv(it->second.fd, buf, len, 0);
	if (received < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;  // No data available
		}
		// Connection errors - disconnect and stop trying
		if (errno == ENOTCONN || errno == ECONNRESET || errno == EPIPE) {
			LOG(Info, "Connection " << conn_handle << " error: " << strerror(errno) << " - disconnecting");
			disconnect(conn_handle);
			return 0;
		}
		LOG(Error, "recv() failed: " << strerror(errno));
		return -1;
	}

	if (received == 0) {
		LOG(Info, "Connection " << conn_handle << " closed by peer");
		disconnect(conn_handle);
		return 0;
	}

	LOG(Debug, "Received " << received << " bytes from connection " << conn_handle);
	return received;
}

int BlueZTransport::set_mtu(uint16_t conn_handle, uint16_t mtu)
{
	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		return -1;
	}

	it->second.mtu = mtu;
	LOG(Debug, "Set MTU to " << mtu << " for connection " << conn_handle);

	if (on_mtu_changed) {
		on_mtu_changed(conn_handle, mtu);
	}

	return 0;
}

uint16_t BlueZTransport::get_mtu(uint16_t conn_handle) const
{
	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		return 23;  // Default
	}
	return it->second.mtu;
}

int BlueZTransport::process_events()
{
	// Check for incoming connections
	accept_l2cap_connection();

	// Check for data on existing connections
	for (auto& pair : connections_) {
		uint8_t buf[512];
		int received = recv_pdu(pair.first, buf, sizeof(buf));
		if (received > 0 && on_data_received) {
			on_data_received(pair.first, buf, received);
		}
	}

	return 0;
}

void BlueZTransport::cleanup()
{
	ENTER();

	// Stop advertising
	if (advertising_) {
		stop_advertising();
	}

	// Close all connections
	for (auto& pair : connections_) {
		close(pair.second.fd);
	}
	connections_.clear();

	// Close sockets
	if (l2cap_listen_fd_ >= 0) {
		close(l2cap_listen_fd_);
		l2cap_listen_fd_ = -1;
	}

	if (hci_fd_ >= 0) {
		close(hci_fd_);
		hci_fd_ = -1;
	}

	LOG(Info, "BlueZTransport cleaned up");
}

} // namespace BLEPP

#endif // BLEPP_SERVER_SUPPORT
