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

#include <blepp/blegattserver.h>
#include <blepp/logging.h>
#include <blepp/att.h>

#ifdef BLEPP_NIMBLE_SUPPORT
#include <blepp/nimble_transport.h>
#endif

#include <algorithm>
#include <cstring>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <unistd.h>  // for usleep()

namespace BLEPP
{

// ATT Opcodes (from att.h, but repeated here for clarity)
#define ATT_OP_ERROR                    0x01
#define ATT_OP_MTU_REQ                  0x02
#define ATT_OP_MTU_RSP                  0x03
#define ATT_OP_FIND_INFO_REQ            0x04
#define ATT_OP_FIND_INFO_RSP            0x05
#define ATT_OP_FIND_BY_TYPE_VALUE_REQ   0x06
#define ATT_OP_FIND_BY_TYPE_VALUE_RSP   0x07
#define ATT_OP_READ_BY_TYPE_REQ         0x08
#define ATT_OP_READ_BY_TYPE_RSP         0x09
#define ATT_OP_READ_REQ                 0x0A
#define ATT_OP_READ_RSP                 0x0B
#define ATT_OP_READ_BLOB_REQ            0x0C
#define ATT_OP_READ_BLOB_RSP            0x0D
#define ATT_OP_READ_MULTIPLE_REQ        0x0E
#define ATT_OP_READ_MULTIPLE_RSP        0x0F
#define ATT_OP_READ_BY_GROUP_TYPE_REQ   0x10
#define ATT_OP_READ_BY_GROUP_TYPE_RSP   0x11
#define ATT_OP_WRITE_REQ                0x12
#define ATT_OP_WRITE_RSP                0x13
#define ATT_OP_WRITE_CMD                0x52
#define ATT_OP_PREPARE_WRITE_REQ        0x16
#define ATT_OP_PREPARE_WRITE_RSP        0x17
#define ATT_OP_EXECUTE_WRITE_REQ        0x18
#define ATT_OP_EXECUTE_WRITE_RSP        0x19
#define ATT_OP_HANDLE_NOTIFY            0x1B
#define ATT_OP_HANDLE_INDICATE          0x1D
#define ATT_OP_HANDLE_CONFIRM           0x1E
#define ATT_OP_SIGNED_WRITE_CMD         0xD2

// Default MTU
#define ATT_DEFAULT_MTU                 23
#define ATT_MAX_MTU                     517

BLEGATTServer::BLEGATTServer(std::unique_ptr<BLETransport> transport)
	: transport_(std::move(transport))
	, running_(false)
{
	ENTER();

	// Set up transport callbacks
	transport_->on_connected = [this](const ConnectionParams& params) {
		this->on_transport_connected(params);
	};

	transport_->on_disconnected = [this](uint16_t conn_handle) {
		this->on_transport_disconnected(conn_handle);
	};

	transport_->on_data_received = [this](uint16_t conn_handle,
	                                      const uint8_t* data, size_t len) {
		this->on_transport_data_received(conn_handle, data, len);
	};

	LOG(Info, "BLEGATTServer created");
}

BLEGATTServer::~BLEGATTServer()
{
	ENTER();
	stop();
}

int BLEGATTServer::register_services(const std::vector<GATTServiceDef>& services)
{
	ENTER();

	// Register with attribute database (for BlueZ transport)
	int rc = db_.register_services(services);
	if (rc != 0) {
		return rc;
	}

	// For NimbleTransport, also register with NimBLE GATTS
#ifdef BLEPP_NIMBLE_SUPPORT
	NimbleTransport* nimble_transport = dynamic_cast<NimbleTransport*>(transport_.get());
	if (nimble_transport) {
		LOG(Info, "Registering services with NimbleTransport");
		rc = nimble_transport->register_services(services);
		if (rc != 0) {
			LOG(Error, "Failed to register services with NimbleTransport: " << rc);
			return rc;
		}
	}
#endif

	return 0;
}

int BLEGATTServer::start_advertising(const AdvertisingParams& params)
{
	ENTER();
	return transport_->start_advertising(params);
}

int BLEGATTServer::stop_advertising()
{
	ENTER();
	return transport_->stop_advertising();
}

bool BLEGATTServer::is_advertising() const
{
	return transport_->is_advertising();
}

int BLEGATTServer::run()
{
	ENTER();

	{
		std::lock_guard<std::mutex> lock(running_mutex_);
		running_ = true;
	}

	LOG(Info, "GATT server running");

	// Accept connections and process events
	while (running_) {
		// Accept new connections (non-blocking or with timeout)
		transport_->accept_connection();

		// Process transport events
		transport_->process_events();

		// Small sleep to prevent busy-wait
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	LOG(Info, "GATT server stopped");
	return 0;
}

void BLEGATTServer::stop()
{
	std::lock_guard<std::mutex> lock(running_mutex_);
	running_ = false;
}

int BLEGATTServer::notify(uint16_t conn_handle, uint16_t char_val_handle,
                         const std::vector<uint8_t>& data)
{
	std::lock_guard<std::mutex> lock(connections_mutex_);

	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		LOG(Error, "Connection " << conn_handle << " not found");
		return -1;
	}

	// Check if client enabled notifications
	uint16_t cccd = it->second.cccd_values[char_val_handle];
	if (!(cccd & 0x0001)) {
		LOG(Warning, "Notifications not enabled for handle " << char_val_handle);
		return -1;
	}

	// Build ATT_OP_HANDLE_NOTIFY PDU
	std::vector<uint8_t> pdu;
	pdu.reserve(3 + data.size());
	pdu.push_back(ATT_OP_HANDLE_NOTIFY);
	pdu.push_back(char_val_handle & 0xFF);
	pdu.push_back((char_val_handle >> 8) & 0xFF);
	pdu.insert(pdu.end(), data.begin(), data.end());

	return transport_->send_pdu(conn_handle, pdu.data(), pdu.size());
}

int BLEGATTServer::indicate(uint16_t conn_handle, uint16_t char_val_handle,
                           const std::vector<uint8_t>& data)
{
	std::lock_guard<std::mutex> lock(connections_mutex_);

	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		LOG(Error, "Connection " << conn_handle << " not found");
		return -1;
	}

	// Check if client enabled indications
	uint16_t cccd = it->second.cccd_values[char_val_handle];
	if (!(cccd & 0x0002)) {
		LOG(Warning, "Indications not enabled for handle " << char_val_handle);
		return -1;
	}

	// Build ATT_OP_HANDLE_INDICATE PDU
	std::vector<uint8_t> pdu;
	pdu.reserve(3 + data.size());
	pdu.push_back(ATT_OP_HANDLE_INDICATE);
	pdu.push_back(char_val_handle & 0xFF);
	pdu.push_back((char_val_handle >> 8) & 0xFF);
	pdu.insert(pdu.end(), data.begin(), data.end());

	// TODO: Wait for ATT_OP_HANDLE_CONFIRM from client
	return transport_->send_pdu(conn_handle, pdu.data(), pdu.size());
}

int BLEGATTServer::disconnect(uint16_t conn_handle)
{
	return transport_->disconnect(conn_handle);
}

ConnectionState* BLEGATTServer::get_connection_state(uint16_t conn_handle)
{
	std::lock_guard<std::mutex> lock(connections_mutex_);

	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		return nullptr;
	}

	return &it->second;
}

// Transport callbacks

void BLEGATTServer::on_transport_connected(const ConnectionParams& params)
{
	ENTER();

	std::lock_guard<std::mutex> lock(connections_mutex_);

	ConnectionState state;
	state.conn_handle = params.conn_handle;
	state.mtu = ATT_DEFAULT_MTU;
	state.connected = true;

	connections_[params.conn_handle] = state;

	LOG(Info, "Client connected: handle=" << params.conn_handle
	          << " addr=" << params.peer_address);

	// Call user callback
	if (on_connected) {
		on_connected(params.conn_handle, params.peer_address);
	}
}

void BLEGATTServer::on_transport_disconnected(uint16_t conn_handle)
{
	ENTER();

	{
		std::lock_guard<std::mutex> lock(connections_mutex_);
		connections_.erase(conn_handle);
	}

	LOG(Info, "Client disconnected: handle=" << conn_handle);

	// Call user callback
	if (on_disconnected) {
		on_disconnected(conn_handle);
	}
}

void BLEGATTServer::on_transport_data_received(uint16_t conn_handle,
                                              const uint8_t* data, size_t len)
{
	if (len < 1) {
		LOG(Error, "Received empty PDU");
		return;
	}

	handle_att_pdu(conn_handle, data, len);
}

// ATT PDU dispatcher

void BLEGATTServer::handle_att_pdu(uint16_t conn_handle,
                                  const uint8_t* pdu, size_t len)
{
	uint8_t opcode = pdu[0];

	LOG(Debug, "ATT PDU: conn=" << conn_handle << " opcode=0x"
	           << std::hex << (int)opcode << std::dec << " len=" << len);

	switch (opcode) {
	case ATT_OP_MTU_REQ:
		handle_mtu_exchange_req(conn_handle, pdu, len);
		break;

	case ATT_OP_FIND_INFO_REQ:
		handle_find_info_req(conn_handle, pdu, len);
		break;

	case ATT_OP_FIND_BY_TYPE_VALUE_REQ:
		handle_find_by_type_value_req(conn_handle, pdu, len);
		break;

	case ATT_OP_READ_BY_TYPE_REQ:
		handle_read_by_type_req(conn_handle, pdu, len);
		break;

	case ATT_OP_READ_REQ:
		handle_read_req(conn_handle, pdu, len);
		break;

	case ATT_OP_READ_BLOB_REQ:
		handle_read_blob_req(conn_handle, pdu, len);
		break;

	case ATT_OP_READ_BY_GROUP_TYPE_REQ:
		handle_read_by_group_type_req(conn_handle, pdu, len);
		break;

	case ATT_OP_WRITE_REQ:
		handle_write_req(conn_handle, pdu, len);
		break;

	case ATT_OP_WRITE_CMD:
		handle_write_cmd(conn_handle, pdu, len);
		break;

	case ATT_OP_PREPARE_WRITE_REQ:
		handle_prepare_write_req(conn_handle, pdu, len);
		break;

	case ATT_OP_EXECUTE_WRITE_REQ:
		handle_execute_write_req(conn_handle, pdu, len);
		break;

	case ATT_OP_SIGNED_WRITE_CMD:
		handle_signed_write_cmd(conn_handle, pdu, len);
		break;

	case ATT_OP_HANDLE_CONFIRM:
		// Confirmation for indication - just acknowledge
		LOG(Debug, "Received indication confirmation");
		break;

	default:
		// Log hex dump of unknown opcode
		std::stringstream hex_dump;
		hex_dump << std::hex << std::setfill('0');
		for (size_t i = 0; i < len && i < 32; i++) {
			hex_dump << std::setw(2) << (int)pdu[i] << " ";
		}
		LOG(Warning, "Unsupported ATT opcode: 0x" << std::hex << (int)opcode
		            << " PDU: " << hex_dump.str());
		send_error_response(conn_handle, opcode, 0x0000, BLE_ATT_ERR_REQ_NOT_SUPPORTED);
		break;
	}
}

// MTU Exchange

void BLEGATTServer::handle_mtu_exchange_req(uint16_t conn_handle,
                                           const uint8_t* pdu, size_t len)
{
	if (len < 3) {
		send_error_response(conn_handle, ATT_OP_MTU_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	uint16_t client_mtu = pdu[1] | (pdu[2] << 8);

	LOG(Debug, "MTU Exchange: client=" << client_mtu);

	// Negotiate MTU (minimum of client and server MTU)
	uint16_t server_mtu = ATT_MAX_MTU;
	uint16_t negotiated_mtu = std::min(client_mtu, server_mtu);

	// Update connection state
	{
		std::lock_guard<std::mutex> lock(connections_mutex_);
		auto it = connections_.find(conn_handle);
		if (it != connections_.end()) {
			it->second.mtu = negotiated_mtu;
		}
	}

	// Update transport MTU
	transport_->set_mtu(conn_handle, negotiated_mtu);

	// Send response
	send_mtu_exchange_rsp(conn_handle, server_mtu);

	LOG(Info, "MTU negotiated: " << negotiated_mtu);

	// Call user callback
	if (on_mtu_exchanged) {
		on_mtu_exchanged(conn_handle, negotiated_mtu);
	}
}

void BLEGATTServer::send_mtu_exchange_rsp(uint16_t conn_handle, uint16_t server_mtu)
{
	uint8_t rsp[3];
	rsp[0] = ATT_OP_MTU_RSP;
	rsp[1] = server_mtu & 0xFF;
	rsp[2] = (server_mtu >> 8) & 0xFF;

	transport_->send_pdu(conn_handle, rsp, sizeof(rsp));
}

// Find Information (UUID discovery)

void BLEGATTServer::handle_find_info_req(uint16_t conn_handle,
                                        const uint8_t* pdu, size_t len)
{
	if (len < 5) {
		send_error_response(conn_handle, ATT_OP_FIND_INFO_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	uint16_t start_handle = pdu[1] | (pdu[2] << 8);
	uint16_t end_handle = pdu[3] | (pdu[4] << 8);

	LOG(Debug, "Find Information: start=0x" << std::hex << start_handle
	           << " end=0x" << end_handle << std::dec);

	if (start_handle == 0 || start_handle > end_handle) {
		send_error_response(conn_handle, ATT_OP_FIND_INFO_REQ, start_handle,
		                   BLE_ATT_ERR_INVALID_HANDLE);
		return;
	}

	// Get all attributes in range
	auto attrs = db_.get_range(start_handle, end_handle);

	if (attrs.empty()) {
		send_error_response(conn_handle, ATT_OP_FIND_INFO_REQ, start_handle,
		                   BLE_ATT_ERR_ATTR_NOT_FOUND);
		return;
	}

	send_find_info_rsp(conn_handle, attrs);
}

void BLEGATTServer::send_find_info_rsp(uint16_t conn_handle,
                                      const std::vector<const Attribute*>& attrs)
{
	if (attrs.empty()) return;

	// Determine format (0x01 = 16-bit UUIDs, 0x02 = 128-bit UUIDs)
	uint8_t format = attrs[0]->uuid.type == BT_UUID16 ? 0x01 : 0x02;
	uint8_t handle_uuid_len = (format == 0x01) ? 4 : 18;

	std::vector<uint8_t> rsp;
	rsp.reserve(2 + attrs.size() * handle_uuid_len);
	rsp.push_back(ATT_OP_FIND_INFO_RSP);
	rsp.push_back(format);

	uint16_t mtu = transport_->get_mtu(conn_handle);
	size_t max_data = mtu - 2;  // Opcode + format

	for (const auto* attr : attrs) {
		// All UUIDs in response must be same size
		if ((attr->uuid.type == BT_UUID16 && format != 0x01) ||
		    (attr->uuid.type == BT_UUID128 && format != 0x02)) {
			break;
		}

		// Check if we have space
		if (rsp.size() + handle_uuid_len > max_data) {
			break;
		}

		// Add handle
		rsp.push_back(attr->handle & 0xFF);
		rsp.push_back((attr->handle >> 8) & 0xFF);

		// Add UUID
		if (format == 0x01) {
			uint16_t uuid16 = attr->uuid.value.u16;
			rsp.push_back(uuid16 & 0xFF);
			rsp.push_back((uuid16 >> 8) & 0xFF);
		} else {
			const uint8_t* uuid128_data = reinterpret_cast<const uint8_t*>(&attr->uuid.value.u128);
			rsp.insert(rsp.end(), uuid128_data, uuid128_data + 16);
		}
	}

	transport_->send_pdu(conn_handle, rsp.data(), rsp.size());
}

// Read By Type (characteristic/descriptor discovery)

void BLEGATTServer::handle_read_by_type_req(uint16_t conn_handle,
                                           const uint8_t* pdu, size_t len)
{
	if (len < 7) {
		send_error_response(conn_handle, ATT_OP_READ_BY_TYPE_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	uint16_t start_handle = pdu[1] | (pdu[2] << 8);
	uint16_t end_handle = pdu[3] | (pdu[4] << 8);

	// Parse UUID (16-bit or 128-bit)
	UUID type_uuid;
	if (len == 7) {
		// 16-bit UUID
		type_uuid = UUID(pdu[5] | (pdu[6] << 8));
	} else if (len == 21) {
		// 128-bit UUID
		type_uuid = UUID(std::vector<uint8_t>(pdu + 5, pdu + 21));
	} else {
		send_error_response(conn_handle, ATT_OP_READ_BY_TYPE_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	LOG(Debug, "Read By Type: start=0x" << std::hex << start_handle
	           << " end=0x" << end_handle << " type=" << type_uuid.str() << std::dec);

	if (start_handle == 0 || start_handle > end_handle) {
		send_error_response(conn_handle, ATT_OP_READ_BY_TYPE_REQ, start_handle,
		                   BLE_ATT_ERR_INVALID_HANDLE);
		return;
	}

	// Find attributes matching type in range
	auto attrs = db_.find_by_type(start_handle, end_handle, type_uuid);

	if (attrs.empty()) {
		send_error_response(conn_handle, ATT_OP_READ_BY_TYPE_REQ, start_handle,
		                   BLE_ATT_ERR_ATTR_NOT_FOUND);
		return;
	}

	send_read_by_type_rsp(conn_handle, attrs);
}

void BLEGATTServer::send_read_by_type_rsp(uint16_t conn_handle,
                                         const std::vector<const Attribute*>& attrs)
{
	if (attrs.empty()) return;

	std::vector<uint8_t> rsp;
	rsp.push_back(ATT_OP_READ_BY_TYPE_RSP);

	uint16_t mtu = transport_->get_mtu(conn_handle);

	// Determine pair length from first attribute
	std::vector<uint8_t> first_value;
	const auto* first_attr = attrs[0];

	// Read attribute value
	if (invoke_read_callback(first_attr, conn_handle, 0, first_value) != 0) {
		first_value = first_attr->value;
	}

	uint8_t pair_len = 2 + first_value.size();  // Handle + value
	rsp.push_back(pair_len);

	size_t max_data = mtu - 2;  // Opcode + length

	for (const auto* attr : attrs) {
		if (rsp.size() + pair_len > max_data) {
			break;
		}

		// Add handle
		rsp.push_back(attr->handle & 0xFF);
		rsp.push_back((attr->handle >> 8) & 0xFF);

		// Add value
		std::vector<uint8_t> value;
		if (invoke_read_callback(attr, conn_handle, 0, value) != 0) {
			value = attr->value;
		}

		// Truncate to pair_len if necessary
		size_t value_len = std::min(value.size(), (size_t)(pair_len - 2));
		rsp.insert(rsp.end(), value.begin(), value.begin() + value_len);
	}

	transport_->send_pdu(conn_handle, rsp.data(), rsp.size());
}

// Read By Group Type (primary service discovery)

void BLEGATTServer::handle_read_by_group_type_req(uint16_t conn_handle,
                                                 const uint8_t* pdu, size_t len)
{
	if (len < 7) {
		send_error_response(conn_handle, ATT_OP_READ_BY_GROUP_TYPE_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	uint16_t start_handle = pdu[1] | (pdu[2] << 8);
	uint16_t end_handle = pdu[3] | (pdu[4] << 8);

	// Parse UUID
	UUID type_uuid;
	if (len == 7) {
		type_uuid = UUID(pdu[5] | (pdu[6] << 8));
	} else if (len == 21) {
		type_uuid = UUID(std::vector<uint8_t>(pdu + 5, pdu + 21));
	} else {
		send_error_response(conn_handle, ATT_OP_READ_BY_GROUP_TYPE_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	// Log request bytes
	std::stringstream req_hex;
	req_hex << std::hex << std::setfill('0');
	for (size_t i = 0; i < len; i++) {
		req_hex << std::setw(2) << (int)pdu[i] << " ";
	}

	LOG(Debug, "Read By Group Type: start=0x" << std::hex << start_handle
	           << " end=0x" << end_handle << " type=" << type_uuid.str() << std::dec
	           << " [" << req_hex.str() << "]");

	// Only Primary Service (0x2800) is a grouping attribute
	if (type_uuid != UUID(0x2800)) {
		send_error_response(conn_handle, ATT_OP_READ_BY_GROUP_TYPE_REQ,
		                   start_handle, BLE_ATT_ERR_UNSUPPORTED_GROUP_TYPE);
		return;
	}

	// Find primary services in range
	auto attrs = db_.find_by_type(start_handle, end_handle, type_uuid);

	LOG(Debug, "Found " << attrs.size() << " services matching type " << type_uuid.str());

	if (attrs.empty()) {
		LOG(Debug, "No services found in range, sending Attribute Not Found error");
		send_error_response(conn_handle, ATT_OP_READ_BY_GROUP_TYPE_REQ,
		                   start_handle, BLE_ATT_ERR_ATTR_NOT_FOUND);
		return;
	}

	// Check if this is a continuation request (start_handle > 1)
	// If so, and we found services, this means we're continuing discovery
	if (start_handle > 1) {
		LOG(Debug, "Continuation request from handle " << start_handle);
	}

	send_read_by_group_type_rsp(conn_handle, attrs);
}

void BLEGATTServer::send_read_by_group_type_rsp(uint16_t conn_handle,
                                               const std::vector<const Attribute*>& attrs)
{
	if (attrs.empty()) return;

	std::vector<uint8_t> rsp;
	rsp.push_back(ATT_OP_READ_BY_GROUP_TYPE_RSP);

	// Determine pair length from first attribute
	// Format: start_handle(2) + end_handle(2) + uuid(2 or 16)
	const auto* first_attr = attrs[0];
	// Check the service UUID size from the value (not the attribute UUID itself)
	uint8_t uuid_size = first_attr->value.size();
	uint8_t pair_len = 4 + uuid_size;
	rsp.push_back(pair_len);

	uint16_t mtu = transport_->get_mtu(conn_handle);

	LOG(Debug, "Building Read By Group Type response: uuid_size=" << (int)uuid_size
	           << " pair_len=" << (int)pair_len << " mtu=" << mtu);

	for (const auto* attr : attrs) {
		// Check if we have room for another pair
		if (rsp.size() + pair_len > mtu) {
			LOG(Debug, "MTU limit reached: rsp.size()=" << rsp.size() << " pair_len=" << (int)pair_len << " mtu=" << mtu);
			break;
		}

		LOG(Debug, "Adding service: handle=" << attr->handle
		           << " end_handle=" << attr->end_group_handle
		           << " value_size=" << attr->value.size());

		// Start handle
		rsp.push_back(attr->handle & 0xFF);
		rsp.push_back((attr->handle >> 8) & 0xFF);

		// End group handle
		rsp.push_back(attr->end_group_handle & 0xFF);
		rsp.push_back((attr->end_group_handle >> 8) & 0xFF);

		// Service UUID (from attribute value)
		if (attr->value.size() >= uuid_size) {
			rsp.insert(rsp.end(), attr->value.begin(), attr->value.begin() + uuid_size);
		} else {
			// Value is smaller than expected - pad with zeros or send error
			LOG(Warning, "Service value size mismatch: expected=" << (int)uuid_size << " actual=" << attr->value.size());
			rsp.insert(rsp.end(), attr->value.begin(), attr->value.end());
			// Pad with zeros if needed
			for (size_t i = attr->value.size(); i < uuid_size; i++) {
				rsp.push_back(0);
			}
		}
	}

	// Validate response format before sending
	size_t expected_size = 2 + (pair_len * (rsp.size() - 2) / pair_len);
	if (rsp.size() != expected_size && rsp.size() >= 2) {
		LOG(Warning, "Response size mismatch: actual=" << rsp.size()
		            << " expected=" << expected_size
		            << " (may indicate incomplete service data)");
	}

	// Log hex dump of response
	std::stringstream hex_dump;
	hex_dump << std::hex << std::setfill('0');
	for (size_t i = 0; i < rsp.size(); i++) {
		hex_dump << std::setw(2) << (int)rsp[i] << " ";
	}
	LOG(Debug, "Sending Read By Group Type response: " << rsp.size() << " bytes: " << hex_dump.str());

	// Additional validation: Check that data length matches what length field claims
	if (rsp.size() >= 2) {
		uint8_t length_field = rsp[1];
		size_t num_entries = (rsp.size() - 2) / length_field;
		size_t expected_total = 2 + (num_entries * length_field);
		LOG(Debug, "Response validation: length_field=" << (int)length_field
		           << " num_entries=" << num_entries
		           << " calculated_size=" << expected_total
		           << " actual_size=" << rsp.size());
	}

	// CRITICAL FIX: Add small delay before sending response
	// Android GATT client has a race condition where it queues the command AFTER sending the request.
	// If our response arrives before Android queues the command (lines 497-498 in att_protocol.cc),
	// gatt_cmd_dequeue() returns NULL and the response is silently dropped.
	// This causes a 5-second timeout and retry.
	// Delay ensures Android has time to queue the command before our response arrives.
	usleep(20000);  // 20ms delay

	transport_->send_pdu(conn_handle, rsp.data(), rsp.size());
}

// Read Request

void BLEGATTServer::handle_read_req(uint16_t conn_handle,
                                   const uint8_t* pdu, size_t len)
{
	if (len < 3) {
		send_error_response(conn_handle, ATT_OP_READ_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	uint16_t handle = pdu[1] | (pdu[2] << 8);

	LOG(Debug, "Read Request: handle=0x" << std::hex << handle << std::dec);

	// Get attribute
	auto* attr = db_.get_attribute(handle);
	if (!attr) {
		send_error_response(conn_handle, ATT_OP_READ_REQ, handle,
		                   BLE_ATT_ERR_INVALID_HANDLE);
		return;
	}

	// Check read permissions
	if (!(attr->permissions & ATT_PERM_READ)) {
		send_error_response(conn_handle, ATT_OP_READ_REQ, handle,
		                   BLE_ATT_ERR_READ_NOT_PERM);
		return;
	}

	// Read value
	std::vector<uint8_t> value;
	int result = invoke_read_callback(attr, conn_handle, 0, value);

	if (result != 0) {
		send_error_response(conn_handle, ATT_OP_READ_REQ, handle, result);
		return;
	}

	send_read_rsp(conn_handle, value);
}

void BLEGATTServer::send_read_rsp(uint16_t conn_handle,
                                 const std::vector<uint8_t>& value)
{
	uint16_t mtu = transport_->get_mtu(conn_handle);
	size_t max_data = mtu - 1;  // Opcode

	std::vector<uint8_t> rsp;
	rsp.reserve(1 + std::min(value.size(), max_data));
	rsp.push_back(ATT_OP_READ_RSP);

	// Truncate to MTU if necessary
	size_t send_len = std::min(value.size(), max_data);
	rsp.insert(rsp.end(), value.begin(), value.begin() + send_len);

	transport_->send_pdu(conn_handle, rsp.data(), rsp.size());
}

// Read Blob Request (for long attributes)

void BLEGATTServer::handle_read_blob_req(uint16_t conn_handle,
                                        const uint8_t* pdu, size_t len)
{
	if (len < 5) {
		send_error_response(conn_handle, ATT_OP_READ_BLOB_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	uint16_t handle = pdu[1] | (pdu[2] << 8);
	uint16_t offset = pdu[3] | (pdu[4] << 8);

	LOG(Debug, "Read Blob Request: handle=0x" << std::hex << handle
	           << " offset=" << std::dec << offset);

	// Get attribute
	auto* attr = db_.get_attribute(handle);
	if (!attr) {
		send_error_response(conn_handle, ATT_OP_READ_BLOB_REQ, handle,
		                   BLE_ATT_ERR_INVALID_HANDLE);
		return;
	}

	// Check read permissions
	if (!(attr->permissions & ATT_PERM_READ)) {
		send_error_response(conn_handle, ATT_OP_READ_BLOB_REQ, handle,
		                   BLE_ATT_ERR_READ_NOT_PERM);
		return;
	}

	// Read value
	std::vector<uint8_t> value;
	int result = invoke_read_callback(attr, conn_handle, offset, value);

	if (result != 0) {
		send_error_response(conn_handle, ATT_OP_READ_BLOB_REQ, handle, result);
		return;
	}

	// Check offset
	if (offset >= value.size()) {
		send_error_response(conn_handle, ATT_OP_READ_BLOB_REQ, handle,
		                   BLE_ATT_ERR_INVALID_OFFSET);
		return;
	}

	// Send from offset
	std::vector<uint8_t> blob_value(value.begin() + offset, value.end());
	send_read_rsp(conn_handle, blob_value);
}

// Write Request

void BLEGATTServer::handle_write_req(uint16_t conn_handle,
                                    const uint8_t* pdu, size_t len)
{
	if (len < 3) {
		send_error_response(conn_handle, ATT_OP_WRITE_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	uint16_t handle = pdu[1] | (pdu[2] << 8);
	std::vector<uint8_t> value(pdu + 3, pdu + len);

	LOG(Debug, "Write Request: handle=0x" << std::hex << handle
	           << std::dec << " len=" << value.size());

	// Get attribute
	auto* attr = db_.get_attribute(handle);
	if (!attr) {
		send_error_response(conn_handle, ATT_OP_WRITE_REQ, handle,
		                   BLE_ATT_ERR_INVALID_HANDLE);
		return;
	}

	// Check write permissions
	if (!(attr->permissions & ATT_PERM_WRITE)) {
		send_error_response(conn_handle, ATT_OP_WRITE_REQ, handle,
		                   BLE_ATT_ERR_WRITE_NOT_PERM);
		return;
	}

	// Special handling for CCCD (0x2902)
	if (attr->uuid == UUID(0x2902) && value.size() == 2) {
		uint16_t cccd_value = value[0] | (value[1] << 8);
		handle_cccd_write(conn_handle, handle, cccd_value);
	}

	// Write value
	int result = invoke_write_callback(attr, conn_handle, value);

	if (result != 0) {
		send_error_response(conn_handle, ATT_OP_WRITE_REQ, handle, result);
		return;
	}

	send_write_rsp(conn_handle);
}

void BLEGATTServer::send_write_rsp(uint16_t conn_handle)
{
	uint8_t rsp = ATT_OP_WRITE_RSP;
	transport_->send_pdu(conn_handle, &rsp, 1);
}

// Write Command (no response)

void BLEGATTServer::handle_write_cmd(uint16_t conn_handle,
                                    const uint8_t* pdu, size_t len)
{
	if (len < 3) {
		// Write command has no response, just ignore
		return;
	}

	uint16_t handle = pdu[1] | (pdu[2] << 8);
	std::vector<uint8_t> value(pdu + 3, pdu + len);

	LOG(Debug, "Write Command: handle=0x" << std::hex << handle
	           << std::dec << " len=" << value.size());

	// Get attribute
	auto* attr = db_.get_attribute(handle);
	if (!attr) {
		// No response for write command
		return;
	}

	// Check write permissions
	if (!(attr->permissions & ATT_PERM_WRITE)) {
		return;
	}

	// Write value (ignore errors for write command)
	invoke_write_callback(attr, conn_handle, value);
}

// Prepare/Execute Write (for long writes)

void BLEGATTServer::handle_prepare_write_req(uint16_t conn_handle,
                                            const uint8_t* pdu, size_t len)
{
	// TODO: Implement prepared write queue
	send_error_response(conn_handle, ATT_OP_PREPARE_WRITE_REQ, 0x0000,
	                   BLE_ATT_ERR_REQ_NOT_SUPPORTED);
}

void BLEGATTServer::handle_execute_write_req(uint16_t conn_handle,
                                            const uint8_t* pdu, size_t len)
{
	// TODO: Implement prepared write queue
	send_error_response(conn_handle, ATT_OP_EXECUTE_WRITE_REQ, 0x0000,
	                   BLE_ATT_ERR_REQ_NOT_SUPPORTED);
}

void BLEGATTServer::handle_signed_write_cmd(uint16_t conn_handle,
                                           const uint8_t* pdu, size_t len)
{
	// TODO: Implement signature verification
	LOG(Warning, "Signed write command not yet supported");
}

// Find By Type Value (for specific service discovery)

void BLEGATTServer::handle_find_by_type_value_req(uint16_t conn_handle,
                                                 const uint8_t* pdu, size_t len)
{
	if (len < 7) {
		send_error_response(conn_handle, ATT_OP_FIND_BY_TYPE_VALUE_REQ, 0x0000,
		                   BLE_ATT_ERR_INVALID_PDU);
		return;
	}

	uint16_t start_handle = pdu[1] | (pdu[2] << 8);
	uint16_t end_handle = pdu[3] | (pdu[4] << 8);
	uint16_t type = pdu[5] | (pdu[6] << 8);
	std::vector<uint8_t> value(pdu + 7, pdu + len);

	LOG(Debug, "Find By Type Value: start=0x" << std::hex << start_handle
	           << " end=0x" << end_handle << " type=0x" << type << std::dec);

	// Find attributes
	auto attrs = db_.find_by_type_value(start_handle, end_handle,
	                                    UUID(type), value);

	if (attrs.empty()) {
		send_error_response(conn_handle, ATT_OP_FIND_BY_TYPE_VALUE_REQ,
		                   start_handle, BLE_ATT_ERR_ATTR_NOT_FOUND);
		return;
	}

	// Build response
	std::vector<uint8_t> rsp;
	rsp.push_back(ATT_OP_FIND_BY_TYPE_VALUE_RSP);

	uint16_t mtu = transport_->get_mtu(conn_handle);
	size_t max_data = mtu - 1;

	for (const auto* attr : attrs) {
		if (rsp.size() + 4 > max_data) {
			break;
		}

		// Found handle
		rsp.push_back(attr->handle & 0xFF);
		rsp.push_back((attr->handle >> 8) & 0xFF);

		// Group end handle
		rsp.push_back(attr->end_group_handle & 0xFF);
		rsp.push_back((attr->end_group_handle >> 8) & 0xFF);
	}

	transport_->send_pdu(conn_handle, rsp.data(), rsp.size());
}

// Error Response

void BLEGATTServer::send_error_response(uint16_t conn_handle, uint8_t opcode,
                                       uint16_t handle, uint8_t error_code)
{
	uint8_t rsp[5];
	rsp[0] = ATT_OP_ERROR;
	rsp[1] = opcode;
	rsp[2] = handle & 0xFF;
	rsp[3] = (handle >> 8) & 0xFF;
	rsp[4] = error_code;

	transport_->send_pdu(conn_handle, rsp, sizeof(rsp));

	LOG(Debug, "ATT Error: opcode=0x" << std::hex << (int)opcode
	           << " handle=0x" << handle
	           << " error=0x" << (int)error_code << std::dec);
}

// CCCD handling

void BLEGATTServer::handle_cccd_write(uint16_t conn_handle, uint16_t cccd_handle,
                                     uint16_t value)
{
	LOG(Debug, "CCCD write: handle=0x" << std::hex << cccd_handle
	           << " value=0x" << value << std::dec);

	// Store CCCD value for this connection
	std::lock_guard<std::mutex> lock(connections_mutex_);

	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		return;
	}

	// CCCD handle is always characteristic value handle + 1
	uint16_t char_handle = cccd_handle - 1;
	it->second.cccd_values[char_handle] = value;

	if (value & 0x0001) {
		LOG(Info, "Notifications enabled for characteristic 0x"
		          << std::hex << char_handle << std::dec);
	}
	if (value & 0x0002) {
		LOG(Info, "Indications enabled for characteristic 0x"
		          << std::hex << char_handle << std::dec);
	}
	if (value == 0x0000) {
		LOG(Info, "Notifications/indications disabled for characteristic 0x"
		          << std::hex << char_handle << std::dec);
	}
}

// Callback invocation helpers

int BLEGATTServer::invoke_read_callback(const Attribute* attr, uint16_t conn_handle,
                                       uint16_t offset, std::vector<uint8_t>& out_data)
{
	if (attr->read_cb) {
		return attr->read_cb(conn_handle, offset, out_data);
	}

	// No callback - use static value
	if (offset >= attr->value.size()) {
		return BLE_ATT_ERR_INVALID_OFFSET;
	}

	out_data.assign(attr->value.begin() + offset, attr->value.end());
	return 0;
}

int BLEGATTServer::invoke_write_callback(Attribute* attr, uint16_t conn_handle,
                                        const std::vector<uint8_t>& data)
{
	if (attr->write_cb) {
		return attr->write_cb(conn_handle, data);
	}

	// No callback - update static value
	attr->value = data;
	return 0;
}

} // namespace BLEPP

#endif // BLEPP_SERVER_SUPPORT
