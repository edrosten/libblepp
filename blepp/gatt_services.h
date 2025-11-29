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

#ifndef __INC_BLEPP_GATT_SERVICES_H
#define __INC_BLEPP_GATT_SERVICES_H

#include <blepp/blepp_config.h>

#ifdef BLEPP_SERVER_SUPPORT

#include <blepp/blestatemachine.h>
#include <blepp/bleattributedb.h>
#include <blepp/att.h>
#include <cstdint>
#include <vector>
#include <functional>

namespace BLEPP
{
	// BLE-style ATT error code aliases (for NimBLE compatibility)
	#define BLE_ATT_ERR_INVALID_HANDLE          ATT_ECODE_INVALID_HANDLE
	#define BLE_ATT_ERR_READ_NOT_PERMITTED      ATT_ECODE_READ_NOT_PERM
	#define BLE_ATT_ERR_READ_NOT_PERM           ATT_ECODE_READ_NOT_PERM
	#define BLE_ATT_ERR_WRITE_NOT_PERMITTED     ATT_ECODE_WRITE_NOT_PERM
	#define BLE_ATT_ERR_WRITE_NOT_PERM          ATT_ECODE_WRITE_NOT_PERM
	#define BLE_ATT_ERR_INVALID_PDU             ATT_ECODE_INVALID_PDU
	#define BLE_ATT_ERR_INSUFFICIENT_AUTHEN     ATT_ECODE_AUTHENTICATION
	#define BLE_ATT_ERR_REQ_NOT_SUPPORTED       ATT_ECODE_REQ_NOT_SUPP
	#define BLE_ATT_ERR_INVALID_OFFSET          ATT_ECODE_INVALID_OFFSET
	#define BLE_ATT_ERR_INSUFFICIENT_AUTHOR     ATT_ECODE_AUTHORIZATION
	#define BLE_ATT_ERR_PREPARE_QUEUE_FULL      ATT_ECODE_PREP_QUEUE_FULL
	#define BLE_ATT_ERR_ATTR_NOT_FOUND          ATT_ECODE_ATTR_NOT_FOUND
	#define BLE_ATT_ERR_ATTR_NOT_LONG           ATT_ECODE_ATTR_NOT_LONG
	#define BLE_ATT_ERR_INSUFFICIENT_KEY_SZ     ATT_ECODE_INSUFF_ENCR_KEY_SIZE
	#define BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN  ATT_ECODE_INVAL_ATTR_VALUE_LEN
	#define BLE_ATT_ERR_UNLIKELY                ATT_ECODE_UNLIKELY
	#define BLE_ATT_ERR_INSUFFICIENT_ENC        ATT_ECODE_INSUFF_ENC
	#define BLE_ATT_ERR_UNSUPPORTED_GROUP       ATT_ECODE_UNSUPP_GRP_TYPE
	#define BLE_ATT_ERR_UNSUPPORTED_GROUP_TYPE  ATT_ECODE_UNSUPP_GRP_TYPE
	#define BLE_ATT_ERR_INSUFFICIENT_RES        ATT_ECODE_INSUFF_RESOURCES


	/// GATT access callback function type
	/// @param conn_handle Connection handle
	/// @param op Access operation (read or write)
	/// @param offset Offset for long reads/writes
	/// @param data Data buffer (input for write, output for read)
	/// @return 0 on success, ATT error code on failure
	using GATTAccessCallback = std::function<int(
		uint16_t conn_handle,
		ATTAccessOp op,
		uint16_t offset,
		std::vector<uint8_t>& data
	)>;

	/// GATT Descriptor Definition
	struct GATTDescriptorDef
	{
		UUID uuid;
		uint8_t permissions;
		GATTAccessCallback access_cb;
		void* arg = nullptr;  // User argument passed to callback

		uint16_t* handle_ptr = nullptr;  // Filled at registration time

		GATTDescriptorDef() : permissions(0) {}

		GATTDescriptorDef(const UUID& u, uint8_t perms,
		                 GATTAccessCallback cb = nullptr)
			: uuid(u), permissions(perms), access_cb(cb)
		{}
	};

	/// GATT Characteristic Definition (NimBLE-compatible)
	struct GATTCharacteristicDef
	{
		UUID uuid;
		uint16_t flags;              // GATT_CHR_F_* flags
		uint8_t min_key_size = 0;    // Minimum required key size
		GATTAccessCallback access_cb;
		void* arg = nullptr;         // User argument passed to callback

		std::vector<GATTDescriptorDef> descriptors;

		uint16_t* val_handle_ptr = nullptr;  // Filled with value handle at registration

		GATTCharacteristicDef() : flags(0) {}

		GATTCharacteristicDef(const UUID& u, uint16_t f,
		                     GATTAccessCallback cb = nullptr)
			: uuid(u), flags(f), access_cb(cb)
		{}
	};

	/// GATT Service Type
	enum class GATTServiceType : uint8_t
	{
		PRIMARY = 1,
		SECONDARY = 2
	};

	/// GATT Service Definition (NimBLE-compatible)
	struct GATTServiceDef
	{
		GATTServiceType type;
		UUID uuid;
		std::vector<GATTCharacteristicDef> characteristics;
		std::vector<uint16_t> included_services;  // Handles of included services

		uint16_t* handle_ptr = nullptr;  // Filled with service handle at registration

		GATTServiceDef() : type(GATTServiceType::PRIMARY) {}

		GATTServiceDef(GATTServiceType t, const UUID& u)
			: type(t), uuid(u)
		{}

		/// Helper: Add a characteristic
		GATTCharacteristicDef& add_characteristic(const UUID& uuid,
		                                          uint16_t flags,
		                                          GATTAccessCallback cb = nullptr)
		{
			characteristics.emplace_back(uuid, flags, cb);
			return characteristics.back();
		}

		/// Helper: Add a read-only characteristic
		GATTCharacteristicDef& add_read_characteristic(const UUID& uuid,
		                                               GATTAccessCallback cb = nullptr)
		{
			return add_characteristic(uuid, GATT_CHR_F_READ, cb);
		}

		/// Helper: Add a read/write characteristic
		GATTCharacteristicDef& add_read_write_characteristic(const UUID& uuid,
		                                                     GATTAccessCallback cb = nullptr)
		{
			return add_characteristic(uuid,
			                         GATT_CHR_F_READ | GATT_CHR_F_WRITE,
			                         cb);
		}

		/// Helper: Add a notify characteristic
		GATTCharacteristicDef& add_notify_characteristic(const UUID& uuid,
		                                                GATTAccessCallback cb = nullptr)
		{
			return add_characteristic(uuid,
			                         GATT_CHR_F_READ | GATT_CHR_F_NOTIFY,
			                         cb);
		}

		/// Helper: Add an indicate characteristic
		GATTCharacteristicDef& add_indicate_characteristic(const UUID& uuid,
		                                                  GATTAccessCallback cb = nullptr)
		{
			return add_characteristic(uuid,
			                         GATT_CHR_F_READ | GATT_CHR_F_INDICATE,
			                         cb);
		}
	};

	/// Helper function: Create a simple read-only service
	inline GATTServiceDef create_read_only_service(const UUID& service_uuid,
	                                               const UUID& char_uuid,
	                                               const std::vector<uint8_t>& value)
	{
		GATTServiceDef service(GATTServiceType::PRIMARY, service_uuid);

		service.add_read_characteristic(char_uuid,
			[value](uint16_t conn_handle, ATTAccessOp op, uint16_t offset,
			       std::vector<uint8_t>& data) -> int {
				if (op == ATTAccessOp::READ_CHR) {
					data = value;
					return 0;
				}
				return BLE_ATT_ERR_UNLIKELY;
			}
		);

		return service;
	}

	/// Helper function: Create a read/write service with callbacks
	inline GATTServiceDef create_read_write_service(
		const UUID& service_uuid,
		const UUID& char_uuid,
		std::function<std::vector<uint8_t>()> read_fn,
		std::function<void(const std::vector<uint8_t>&)> write_fn)
	{
		GATTServiceDef service(GATTServiceType::PRIMARY, service_uuid);

		service.add_read_write_characteristic(char_uuid,
			[read_fn, write_fn](uint16_t conn_handle, ATTAccessOp op,
			                   uint16_t offset, std::vector<uint8_t>& data) -> int {
				if (op == ATTAccessOp::READ_CHR) {
					data = read_fn();
					return 0;
				} else if (op == ATTAccessOp::WRITE_CHR) {
					write_fn(data);
					return 0;
				}
				return BLE_ATT_ERR_UNLIKELY;
			}
		);

		return service;
	}

} // namespace BLEPP

#endif // BLEPP_SERVER_SUPPORT
#endif // __INC_BLEPP_GATT_SERVICES_H
