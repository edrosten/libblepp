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

#ifndef __INC_BLEPP_GATT_SERVER_H
#define __INC_BLEPP_GATT_SERVER_H

#include <blepp/blepp_config.h>

#ifdef BLEPP_SERVER_SUPPORT

#include <blepp/bletransport.h>
#include <blepp/bleattributedb.h>
#include <blepp/gatt_services.h>
#include <memory>
#include <map>
#include <mutex>
#include <functional>
#include <chrono>

namespace BLEPP
{
	/// Per-connection state for GATT server
	struct ConnectionState
	{
		uint16_t conn_handle;
		uint16_t mtu;                              ///< Negotiated MTU (default 23)
		std::map<uint16_t, uint16_t> cccd_values;  ///< CCCD values per characteristic
		bool connected;
		std::chrono::steady_clock::time_point connection_time;  ///< When connection was established
	};

	/// BLE GATT Server
	/// Implements a complete GATT server using an attribute database and transport layer
	class BLEGATTServer
	{
	public:
		/// Constructor
		/// @param transport Transport implementation (BlueZ or ATBM)
		explicit BLEGATTServer(std::unique_ptr<BLETransport> transport);

		/// Destructor
		~BLEGATTServer();

		/// Get the attribute database
		/// @return Reference to the attribute database
		BLEAttributeDatabase& db() { return db_; }
		const BLEAttributeDatabase& db() const { return db_; }

		/// Register services from definitions
		/// @param services Vector of service definitions
		/// @return 0 on success, negative on error
		int register_services(const std::vector<GATTServiceDef>& services);

		/// Start advertising
		/// @param params Advertising parameters
		/// @return 0 on success, negative on error
		int start_advertising(const AdvertisingParams& params);

		/// Stop advertising
		/// @return 0 on success, negative on error
		int stop_advertising();

		/// Check if advertising
		/// @return true if advertising
		bool is_advertising() const;

		/// Run the server event loop
		/// This blocks and processes events. Call from main thread or dedicated thread.
		/// @return 0 on normal exit, negative on error
		int run();

		/// Stop the server event loop
		void stop();

		/// Send notification to a client
		/// @param conn_handle Connection handle
		/// @param char_val_handle Characteristic value handle
		/// @param data Notification data
		/// @return 0 on success, negative on error
		int notify(uint16_t conn_handle, uint16_t char_val_handle,
		          const std::vector<uint8_t>& data);

		/// Send indication to a client (with acknowledgment)
		/// @param conn_handle Connection handle
		/// @param char_val_handle Characteristic value handle
		/// @param data Indication data
		/// @return 0 on success, negative on error
		int indicate(uint16_t conn_handle, uint16_t char_val_handle,
		            const std::vector<uint8_t>& data);

		/// Disconnect a client
		/// @param conn_handle Connection handle
		/// @return 0 on success, negative on error
		int disconnect(uint16_t conn_handle);

		/// Get connection state
		/// @param conn_handle Connection handle
		/// @return Pointer to connection state, or nullptr if not found
		ConnectionState* get_connection_state(uint16_t conn_handle);

		/// Callbacks

		/// Called when a client connects
		std::function<void(uint16_t conn_handle, const std::string& peer_addr)> on_connected;

		/// Called when a client disconnects
		std::function<void(uint16_t conn_handle)> on_disconnected;

		/// Called when MTU is exchanged
		std::function<void(uint16_t conn_handle, uint16_t mtu)> on_mtu_exchanged;

	private:
		std::unique_ptr<BLETransport> transport_;
		BLEAttributeDatabase db_;

		std::mutex connections_mutex_;
		std::map<uint16_t, ConnectionState> connections_;

		bool running_;
		std::mutex running_mutex_;

		// ATT PDU handlers

		/// Handle incoming ATT PDU
		/// @param conn_handle Connection handle
		/// @param pdu PDU data
		/// @param len PDU length
		void handle_att_pdu(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle MTU Exchange Request (0x02)
		void handle_mtu_exchange_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Find Information Request (0x04)
		void handle_find_info_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Find By Type Value Request (0x06)
		void handle_find_by_type_value_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Read By Type Request (0x08)
		void handle_read_by_type_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Read Request (0x0A)
		void handle_read_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Read Blob Request (0x0C)
		void handle_read_blob_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Read By Group Type Request (0x10)
		void handle_read_by_group_type_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Write Request (0x12)
		void handle_write_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Write Command (0x52)
		void handle_write_cmd(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Prepare Write Request (0x16)
		void handle_prepare_write_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Execute Write Request (0x18)
		void handle_execute_write_req(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		/// Handle Signed Write Command (0xD2)
		void handle_signed_write_cmd(uint16_t conn_handle, const uint8_t* pdu, size_t len);

		// Response builders

		/// Send ATT Error Response
		/// @param conn_handle Connection handle
		/// @param opcode Request opcode that caused error
		/// @param handle Attribute handle
		/// @param error_code ATT error code
		void send_error_response(uint16_t conn_handle, uint8_t opcode,
		                        uint16_t handle, uint8_t error_code);

		/// Send MTU Exchange Response
		void send_mtu_exchange_rsp(uint16_t conn_handle, uint16_t server_mtu);

		/// Send Find Information Response
		void send_find_info_rsp(uint16_t conn_handle,
		                       const std::vector<const Attribute*>& attrs);

		/// Send Read By Type Response
		void send_read_by_type_rsp(uint16_t conn_handle,
		                          const std::vector<const Attribute*>& attrs);

		/// Send Read Response
		void send_read_rsp(uint16_t conn_handle, const std::vector<uint8_t>& value);

		/// Send Read By Group Type Response
		void send_read_by_group_type_rsp(uint16_t conn_handle,
		                                const std::vector<const Attribute*>& attrs);

		/// Send Write Response
		void send_write_rsp(uint16_t conn_handle);

		// Helper methods

		/// Handle CCCD write
		/// @param conn_handle Connection handle
		/// @param cccd_handle CCCD handle
		/// @param value New CCCD value (0x0000, 0x0001, 0x0002)
		void handle_cccd_write(uint16_t conn_handle, uint16_t cccd_handle,
		                      uint16_t value);

		/// Invoke attribute read callback if present
		/// @param attr Attribute
		/// @param conn_handle Connection handle
		/// @param offset Read offset
		/// @param out_data Output data buffer
		/// @return 0 on success, ATT error code on failure
		int invoke_read_callback(const Attribute* attr, uint16_t conn_handle,
		                        uint16_t offset, std::vector<uint8_t>& out_data);

		/// Invoke attribute write callback if present
		/// @param attr Attribute
		/// @param conn_handle Connection handle
		/// @param data Data to write
		/// @return 0 on success, ATT error code on failure
		int invoke_write_callback(Attribute* attr, uint16_t conn_handle,
		                         const std::vector<uint8_t>& data);

		/// Transport callbacks
		void on_transport_connected(const ConnectionParams& params);
		void on_transport_disconnected(uint16_t conn_handle);
		void on_transport_data_received(uint16_t conn_handle,
		                               const uint8_t* data, size_t len);
	};

} // namespace BLEPP

#endif // BLEPP_SERVER_SUPPORT
#endif // __INC_BLEPP_GATT_SERVER_H
