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

#ifdef BLEPP_BLUEZ_SUPPORT

#include <blepp/bluez_client_transport.h>
#include <blepp/logging.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <algorithm>

namespace BLEPP
{

BlueZClientTransport::BlueZClientTransport()
	: hci_dev_id_(-1)
	, hci_fd_(-1)
	, scanning_(false)
{
	ENTER();
}

BlueZClientTransport::~BlueZClientTransport()
{
	ENTER();

	if (scanning_) {
		stop_scan();
	}

	// Close all connections
	for (auto& conn : connections_) {
		if (conn.second.fd >= 0) {
			close(conn.second.fd);
		}
	}
	connections_.clear();

	close_hci_device();
}

bool BlueZClientTransport::is_available() const
{
	LOG(Debug, "BlueZClientTransport::is_available() - checking availability");

	// Check if we can open HCI device
	int dev_id = hci_get_route(nullptr);
	LOG(Debug, "hci_get_route(nullptr) returned: " << dev_id);

	if (dev_id < 0) {
		LOG(Debug, "hci_get_route failed with errno=" << errno << " (" << strerror(errno) << "), trying device 0");
		dev_id = 0;
	}

	// Try to bring up the device
	int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (ctl < 0) {
		LOG(Error, "Failed to create HCI socket for HCIDEVUP: " << strerror(errno));
	} else {
		LOG(Debug, "Created HCI control socket, attempting HCIDEVUP on device " << dev_id);
		if (ioctl(ctl, HCIDEVUP, dev_id) < 0) {
			if (errno == EALREADY) {
				LOG(Debug, "Device hci" << dev_id << " already up");
			} else {
				LOG(Warning, "HCIDEVUP failed for hci" << dev_id << ": " << strerror(errno));
			}
		} else {
			LOG(Info, "Successfully brought up hci" << dev_id);
		}
		close(ctl);

		// After bringing up the device, get the route again to confirm it's available
		LOG(Debug, "Re-checking hci_get_route() after HCIDEVUP...");
		int new_dev_id = hci_get_route(nullptr);
		if (new_dev_id >= 0) {
			dev_id = new_dev_id;
			LOG(Debug, "hci_get_route() now returns: " << dev_id);
		}
	}

	// Try to open the device
	LOG(Debug, "Attempting to open HCI device " << dev_id);
	int fd = hci_open_dev(dev_id);
	if (fd < 0) {
		LOG(Error, "hci_open_dev(" << dev_id << ") failed: " << strerror(errno));
		return false;
	}

	LOG(Info, "BlueZ transport is available (hci" << dev_id << ")");
	close(fd);
	return true;
}

int BlueZClientTransport::open_hci_device()
{
	ENTER();

	if (hci_fd_ >= 0) {
		return 0;  // Already open
	}

	// Get HCI device ID
	hci_dev_id_ = hci_get_route(nullptr);
	if (hci_dev_id_ < 0) {
		// hci_get_route failed - try device 0 directly
		LOG(Debug, "hci_get_route failed, trying hci0 directly");
		hci_dev_id_ = 0;
	}

	// Bring up the HCI device if it's down
	// This is equivalent to "hciconfig hci0 up"
	int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
	if (ctl >= 0) {
		if (ioctl(ctl, HCIDEVUP, hci_dev_id_) < 0) {
			if (errno != EALREADY) {
				LOG(Warning, "Failed to bring up HCI device " << hci_dev_id_
				          << ": " << strerror(errno));
			}
		} else {
			LOG(Info, "Brought up HCI device " << hci_dev_id_);
		}
		close(ctl);

		// After bringing up the device, get the route again to confirm it's available
		int new_dev_id = hci_get_route(nullptr);
		if (new_dev_id >= 0) {
			hci_dev_id_ = new_dev_id;
			LOG(Debug, "hci_get_route() now returns: " << hci_dev_id_);
		}
	}

	// Now try to open the device
	hci_fd_ = hci_open_dev(hci_dev_id_);
	if (hci_fd_ < 0) {
		LOG(Error, "Failed to open HCI device " << hci_dev_id_
		          << ": " << strerror(errno));
		return -1;
	}

	// Set up HCI filter to receive LE meta events
	struct hci_filter flt;
	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_set_event(EVT_LE_META_EVENT, &flt);
	hci_filter_set_event(EVT_CMD_COMPLETE, &flt);
	hci_filter_set_event(EVT_CMD_STATUS, &flt);

	if (setsockopt(hci_fd_, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		LOG(Warning, "Failed to set HCI filter: " << strerror(errno));
	} else {
		LOG(Debug, "HCI filter set to receive LE meta events");
	}

	LOG(Debug, "Opened HCI device " << hci_dev_id_ << " (fd=" << hci_fd_ << ")");
	return 0;
}

void BlueZClientTransport::close_hci_device()
{
	ENTER();

	if (hci_fd_ >= 0) {
		hci_close_dev(hci_fd_);
		hci_fd_ = -1;
		hci_dev_id_ = -1;
	}
}

int BlueZClientTransport::start_scan(const ScanParams& params)
{
	ENTER();
	LOG(Info, "start_scan() called");

	if (scanning_) {
		LOG(Warning, "Already scanning");
		return 0;
	}

	LOG(Debug, "Opening HCI device for scanning...");
	if (open_hci_device() < 0) {
		LOG(Error, "Failed to open HCI device");
		return -1;
	}

	scan_params_ = params;
	LOG(Debug, "Scan params: type=" << (int)params.scan_type
	          << " interval=" << params.interval_ms << "ms"
	          << " window=" << params.window_ms << "ms"
	          << " filter_duplicates=" << (int)params.filter_duplicates);

	// Set scan parameters
	LOG(Debug, "Setting scan parameters...");
	if (set_scan_parameters(params) < 0) {
		LOG(Error, "Failed to set scan parameters");
		return -1;
	}

	// Enable scanning - hardware filtering only when Hardware mode is selected
	LOG(Debug, "Enabling scanning...");
	bool hw_filter = (params.filter_duplicates == ScanParams::FilterDuplicates::Hardware);
	if (set_scan_enable(true, hw_filter) < 0) {
		LOG(Error, "Failed to enable scanning");
		return -1;
	}

	scanning_ = true;
	seen_devices_.clear();

	LOG(Info, "BLE scanning started successfully on hci" << hci_dev_id_ << " (fd=" << hci_fd_ << ")");
	return 0;
}

int BlueZClientTransport::stop_scan()
{
	ENTER();

	if (!scanning_) {
		return 0;
	}

	if (set_scan_enable(false, false) < 0) {
		LOG(Warning, "Failed to disable scanning");
	}

	scanning_ = false;
	close_hci_device();
	LOG(Info, "BLE scanning stopped and HCI device closed");
	return 0;
}

int BlueZClientTransport::set_scan_parameters(const ScanParams& params)
{
	ENTER();

	uint8_t scan_type = static_cast<uint8_t>(params.scan_type);
	uint16_t interval = params.interval_ms * 1000 / 625;  // Convert to 0.625ms units
	uint16_t window = params.window_ms * 1000 / 625;
	uint8_t own_type = 0x00;  // Public address
	uint8_t filter = static_cast<uint8_t>(params.filter_policy);

	if (hci_le_set_scan_parameters(hci_fd_, scan_type, htobs(interval),
	                                htobs(window), own_type, filter, 1000) < 0) {
		LOG(Error, "Failed to set scan parameters: " << strerror(errno));
		return -1;
	}

	LOG(Debug, "Set scan parameters: interval=" << params.interval_ms
	          << "ms window=" << params.window_ms << "ms");
	return 0;
}

int BlueZClientTransport::set_scan_enable(bool enable, bool filter_duplicates)
{
	ENTER();

	uint8_t enable_val = enable ? 0x01 : 0x00;
	uint8_t filter_dup = filter_duplicates ? 0x01 : 0x00;

	if (hci_le_set_scan_enable(hci_fd_, enable_val, filter_dup, 1000) < 0) {
		LOG(Error, "Failed to " << (enable ? "enable" : "disable")
		          << " scanning: " << strerror(errno));
		return -1;
	}

	return 0;
}

int BlueZClientTransport::get_advertisements(std::vector<AdvertisementData>& ads, int timeout_ms)
{
	ENTER();
	static int call_count = 0;
	static auto last_log = std::chrono::steady_clock::now();
	call_count++;

	// Log every 30 seconds
	auto now = std::chrono::steady_clock::now();
	if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() >= 30) {
		LOG(Debug, "Scanner status: " << call_count << " polls, scanning=" << scanning_);
		last_log = now;
	}

	if (!scanning_) {
		LOG(Error, "Not scanning - scan state is false!");
		return -1;
	}

	ads.clear();
	return read_hci_events(ads, timeout_ms);
}

int BlueZClientTransport::read_hci_events(std::vector<AdvertisementData>& ads, int timeout_ms)
{
	ENTER();
	static int select_count = 0;
	static int event_count = 0;
	static int ad_count = 0;

	select_count++;

	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(hci_fd_, &rfds);

	if (timeout_ms >= 0) {
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
	}

	int ret = select(hci_fd_ + 1, &rfds, nullptr, nullptr,
	                 timeout_ms >= 0 ? &tv : nullptr);

	if (ret < 0) {
		LOG(Error, "select() failed: " << strerror(errno));
		return -1;
	}

	if (ret == 0) {
		// Timeout - no events available
		return 0;
	}

	// Read HCI event
	uint8_t buf[HCI_MAX_EVENT_SIZE];
	ssize_t len = read(hci_fd_, buf, sizeof(buf));

	if (len < 0) {
		LOG(Error, "read() failed: " << strerror(errno));
		return -1;
	}

	event_count++;

	// The first byte is the HCI packet type (0x04 = HCI_EVENT_PKT)
	// Skip it and parse the actual event starting at buf[1]
	if (len < 1 + HCI_EVENT_HDR_SIZE) {
		LOG(Warning, "Packet too short: " << len << " bytes");
		return 0;
	}

	// Parse HCI event (skip the packet type byte)
	hci_event_hdr* hdr = (hci_event_hdr*)(buf + 1);
	len -= 1;  // Adjust length to account for skipped packet type byte

	// Log occasionally for debugging
	if (event_count % 1000 == 1) {
		LOG(Debug, "HCI event stats: " << event_count << " events, "
		          << ad_count << " advertisements received");
	}

	if (hdr->evt != EVT_LE_META_EVENT) {
		// Not an LE meta event
		return 0;
	}

	// Parse LE meta event
	// buf now points to: [Event Code][Param Len][Subevent Code][Data...]
	// hdr = (hci_event_hdr*)(buf + 1), so:
	//   hdr->evt = buf[1] = Event Code (0x3e)
	//   hdr->plen = buf[2] = Param Length
	// Subevent starts at buf[1] + sizeof(hci_event_hdr) = buf[1] + 2 = buf[3]
	// So we need: buf + 1 + 2 for the subevent data
	int num_ads = parse_advertising_report(buf + 3,  // Skip: pkt_type(1) + evt(1) + plen(1)
	                                       len - 2, ads);  // Adjust length
	if (num_ads > 0) {
		ad_count += num_ads;
		LOG(Debug, "Received " << num_ads << " advertisement(s), total=" << ad_count);
	}
	return num_ads;
}

int BlueZClientTransport::parse_advertising_report(const uint8_t* data, size_t len,
                                                   std::vector<AdvertisementData>& ads)
{
	if (len < 1) {
		LOG(Warning, "parse_advertising_report: packet too short: " << len << " bytes");
		return 0;
	}

	uint8_t subevent = data[0];

	if (subevent != EVT_LE_ADVERTISING_REPORT) {
		LOG(Debug, "Ignoring non-advertising LE subevent: 0x" << std::hex << (int)subevent << std::dec);
		return 0;
	}

	if (len < 2) return 0;

	uint8_t num_reports = data[1];
	const uint8_t* ptr = data + 2;
	const uint8_t* end = data + len;

	for (uint8_t i = 0; i < num_reports && ptr < end; i++) {
		if (ptr + 10 > end) break;  // Minimum report size

		AdvertisementData ad;
		ad.event_type = ptr[0];
		ad.address_type = ptr[1];

		// Parse address (6 bytes, little-endian)
		char addr_str[18];
		ba2str((bdaddr_t*)(ptr + 2), addr_str);
		ad.address = addr_str;

		uint8_t data_len = ptr[8];
		ptr += 9;

		if (ptr + data_len + 1 > end) break;

		// Copy advertising data
		ad.data.assign(ptr, ptr + data_len);
		ptr += data_len;

		// RSSI
		ad.rssi = (int8_t)(*ptr);
		ptr++;

		// Apply software duplicate filtering if Software mode is selected
		if (scan_params_.filter_duplicates == ScanParams::FilterDuplicates::Software) {
			if (seen_devices_.count(ad.address) > 0) {
				continue;  // Skip duplicate
			}
			seen_devices_.insert(ad.address);
		}

		ads.push_back(ad);

		// Call callback if set
		if (on_advertisement) {
			on_advertisement(ad);
		}
	}

	return ads.size();
}

int BlueZClientTransport::connect(const ClientConnectionParams& params)
{
	ENTER();

	// Create L2CAP socket
	int sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (sock < 0) {
		LOG(Error, "Failed to create L2CAP socket: " << strerror(errno));
		return -1;
	}

	// Set up address
	struct sockaddr_l2 addr = {};
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_cid = htobs(4);  // ATT CID
	addr.l2_bdaddr_type = params.peer_address_type;

	// Parse address string to bdaddr_t
	if (str2ba(params.peer_address.c_str(), &addr.l2_bdaddr) < 0) {
		LOG(Error, "Invalid Bluetooth address: " << params.peer_address);
		close(sock);
		return -1;
	}

	// Connect
	if (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		LOG(Error, "Failed to connect to " << params.peer_address
		          << ": " << strerror(errno));
		close(sock);
		return -1;
	}

	// Store connection info
	ConnectionInfo info;
	info.fd = sock;
	info.mtu = 23;  // Default ATT MTU
	info.address = params.peer_address;

	connections_[sock] = info;

	LOG(Info, "Connected to " << params.peer_address << " (fd=" << sock << ")");

	// Call callback if set
	if (on_connected) {
		on_connected(sock);
	}

	return sock;
}

int BlueZClientTransport::disconnect(int fd)
{
	ENTER();

	auto it = connections_.find(fd);
	if (it == connections_.end()) {
		LOG(Warning, "Unknown connection fd=" << fd);
		return -1;
	}

	std::string addr = it->second.address;
	close(fd);
	connections_.erase(it);

	LOG(Info, "Disconnected from " << addr << " (fd=" << fd << ")");

	// Call callback if set
	if (on_disconnected) {
		on_disconnected(fd);
	}

	return 0;
}

int BlueZClientTransport::get_fd(int fd) const
{
	// In BlueZ, the connection fd IS the file descriptor for select/poll
	return fd;
}

int BlueZClientTransport::send(int fd, const uint8_t* data, size_t len)
{
	auto it = connections_.find(fd);
	if (it == connections_.end()) {
		LOG(Error, "Invalid connection fd=" << fd);
		return -1;
	}

	ssize_t sent = write(fd, data, len);
	if (sent < 0) {
		LOG(Error, "Failed to send data on fd=" << fd << ": " << strerror(errno));
		return -1;
	}

	return sent;
}

int BlueZClientTransport::receive(int fd, uint8_t* data, size_t max_len)
{
	auto it = connections_.find(fd);
	if (it == connections_.end()) {
		LOG(Error, "Invalid connection fd=" << fd);
		return -1;
	}

	ssize_t received = read(fd, data, max_len);
	if (received < 0) {
		LOG(Error, "Failed to receive data on fd=" << fd << ": " << strerror(errno));
		return -1;
	}

	if (received == 0) {
		// Connection closed
		LOG(Info, "Connection closed by peer (fd=" << fd << ")");
		return 0;
	}

	// Call callback if set
	if (on_data_received) {
		on_data_received(fd, data, received);
	}

	return received;
}

uint16_t BlueZClientTransport::get_mtu(int fd) const
{
	auto it = connections_.find(fd);
	if (it == connections_.end()) {
		return 23;  // Default ATT MTU
	}

	return it->second.mtu;
}

int BlueZClientTransport::set_mtu(int fd, uint16_t mtu)
{
	auto it = connections_.find(fd);
	if (it == connections_.end()) {
		LOG(Error, "Invalid connection fd=" << fd);
		return -1;
	}

	it->second.mtu = mtu;
	LOG(Debug, "Set MTU=" << mtu << " for fd=" << fd);
	return 0;
}

// ============================================================================
// MAC Address Operations
// ============================================================================

std::string BlueZClientTransport::get_mac_address() const
{
	ENTER();

	// Return cached address if available
	if (!mac_address_.empty()) {
		return mac_address_;
	}

	// Read address from HCI device
	bdaddr_t bdaddr;
	int rc = hci_devba(hci_dev_id_, &bdaddr);
	if (rc < 0) {
		LOG(Error, "Failed to read device address");
		return "";
	}

	// Convert bdaddr_t to string (BlueZ format is little-endian)
	char addr_str[18];
	ba2str(&bdaddr, addr_str);
	mac_address_ = addr_str;

	return mac_address_;
}

} // namespace BLEPP

#endif // BLEPP_BLUEZ_SUPPORT
