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

#include <blepp/bleattributedb.h>
#include <blepp/gatt_services.h>
#include <blepp/logging.h>
#include <blepp/att.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace BLEPP
{

// Standard GATT UUIDs
static const UUID UUID_PRIMARY_SERVICE(0x2800);
static const UUID UUID_SECONDARY_SERVICE(0x2801);
static const UUID UUID_INCLUDE(0x2802);
static const UUID UUID_CHARACTERISTIC(0x2803);
static const UUID UUID_CCCD(0x2902);  // Client Characteristic Configuration Descriptor

BLEAttributeDatabase::BLEAttributeDatabase()
	: next_handle_(1)  // Handles start at 1 (0 is invalid)
{
	ENTER();
}

BLEAttributeDatabase::~BLEAttributeDatabase()
{
	ENTER();
	clear();
}

uint16_t BLEAttributeDatabase::allocate_handle()
{
	if (next_handle_ == 0xFFFF) {
		LOG(Error, "Handle space exhausted!");
		return 0;
	}
	return next_handle_++;
}

uint8_t BLEAttributeDatabase::flags_to_properties(uint16_t flags)
{
	uint8_t props = 0;

	if (flags & GATT_CHR_F_BROADCAST) props |= GATT_CHR_PROP_BROADCAST;
	if (flags & GATT_CHR_F_READ) props |= GATT_CHR_PROP_READ;
	if (flags & GATT_CHR_F_WRITE_NO_RSP) props |= GATT_CHR_PROP_WRITE_NO_RSP;
	if (flags & GATT_CHR_F_WRITE) props |= GATT_CHR_PROP_WRITE;
	if (flags & GATT_CHR_F_NOTIFY) props |= GATT_CHR_PROP_NOTIFY;
	if (flags & GATT_CHR_F_INDICATE) props |= GATT_CHR_PROP_INDICATE;
	if (flags & GATT_CHR_F_AUTH_SIGN_WRITE) props |= GATT_CHR_PROP_AUTH_WRITE;

	return props;
}

uint8_t BLEAttributeDatabase::flags_to_permissions(uint16_t flags)
{
	uint8_t perms = 0;

	if (flags & GATT_CHR_F_READ) perms |= ATT_PERM_READ;
	if (flags & (GATT_CHR_F_WRITE | GATT_CHR_F_WRITE_NO_RSP)) perms |= ATT_PERM_WRITE;
	if (flags & GATT_CHR_F_READ_ENC) perms |= ATT_PERM_READ_ENCRYPT;
	if (flags & GATT_CHR_F_WRITE_ENC) perms |= ATT_PERM_WRITE_ENCRYPT;
	if (flags & GATT_CHR_F_READ_AUTHEN) perms |= ATT_PERM_READ_AUTHEN;
	if (flags & GATT_CHR_F_WRITE_AUTHEN) perms |= ATT_PERM_WRITE_AUTHEN;

	return perms;
}

uint16_t BLEAttributeDatabase::add_primary_service(const UUID& uuid)
{
	ENTER();

	uint16_t handle = allocate_handle();
	if (handle == 0) return 0;

	Attribute attr = {};  // Zero-initialize all fields
	attr.handle = handle;
	attr.type = AttributeType::PRIMARY_SERVICE;
	attr.uuid = UUID_PRIMARY_SERVICE;
	attr.permissions = ATT_PERM_READ;
	attr.end_group_handle = handle;  // Initialize to service handle, will be updated later

	// Value is the service UUID
	if (uuid.type == BT_UUID16) {
		uint16_t val = uuid.value.u16;
		attr.value = {(uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF)};
	} else {
		// 128-bit UUID - Our UUID parsing stores in little-endian, ATT also requires little-endian
		// So we just copy directly without reversing
		attr.value.resize(16);
		memcpy(attr.value.data(), uuid.value.u128.data, 16);

		// Debug: log the UUID bytes
		std::stringstream uuid_hex;
		uuid_hex << std::hex << std::setfill('0');
		for (int i = 0; i < 16; i++) {
			uuid_hex << std::setw(2) << (int)attr.value[i] << " ";
		}
		LOG(Info, "Stored primary service UUID bytes (little-endian for ATT): " << uuid_hex.str());
	}

	attributes_[handle] = attr;

	// Track service for end_group_handle updates
	services_.push_back({handle, handle});

	LOG(Info, "Added primary service " << uuid.str() << " at handle " << handle);
	return handle;
}

uint16_t BLEAttributeDatabase::add_secondary_service(const UUID& uuid)
{
	ENTER();

	uint16_t handle = allocate_handle();
	if (handle == 0) return 0;

	Attribute attr = {};  // Zero-initialize all fields
	attr.handle = handle;
	attr.type = AttributeType::SECONDARY_SERVICE;
	attr.uuid = UUID_SECONDARY_SERVICE;
	attr.permissions = ATT_PERM_READ;
	attr.end_group_handle = handle;  // Initialize to service handle, will be updated later

	// Value is the service UUID
	if (uuid.type == BT_UUID16) {
		uint16_t val = uuid.value.u16;
		attr.value = {(uint8_t)(val & 0xFF), (uint8_t)((val >> 8) & 0xFF)};
	} else {
		// 128-bit UUID - ATT requires little-endian, which is how we store it
		attr.value.resize(16);
		memcpy(attr.value.data(), uuid.value.u128.data, 16);

		// Debug: log the UUID bytes
		std::stringstream uuid_hex;
		uuid_hex << std::hex << std::setfill('0');
		for (int i = 0; i < 16; i++) {
			uuid_hex << std::setw(2) << (int)attr.value[i] << " ";
		}
		LOG(Info, "Stored secondary service UUID bytes (little-endian for ATT): " << uuid_hex.str());
	}

	attributes_[handle] = attr;

	services_.push_back({handle, handle});

	LOG(Info, "Added secondary service " << uuid.str() << " at handle " << handle);
	return handle;
}

uint16_t BLEAttributeDatabase::add_include(uint16_t service_handle,
                                           uint16_t included_service_handle)
{
	ENTER();

	uint16_t handle = allocate_handle();
	if (handle == 0) return 0;

	// Find the included service to get its end handle and UUID
	auto inc_svc = get_attribute(included_service_handle);
	if (!inc_svc) {
		LOG(Error, "Included service handle " << included_service_handle << " not found");
		return 0;
	}

	Attribute attr;
	attr.handle = handle;
	attr.type = AttributeType::INCLUDE;
	attr.uuid = UUID_INCLUDE;
	attr.permissions = ATT_PERM_READ;

	// Value format: included_handle (2 bytes) + end_group_handle (2 bytes) + UUID (0 or 2 bytes)
	attr.value.resize(4);
	attr.value[0] = included_service_handle & 0xFF;
	attr.value[1] = (included_service_handle >> 8) & 0xFF;
	attr.value[2] = inc_svc->end_group_handle & 0xFF;
	attr.value[3] = (inc_svc->end_group_handle >> 8) & 0xFF;

	// If the included service has a 16-bit UUID, append it
	if (inc_svc->uuid.type == BT_UUID16) {
		uint16_t uuid16 = inc_svc->uuid.value.u16;
		attr.value.push_back(uuid16 & 0xFF);
		attr.value.push_back((uuid16 >> 8) & 0xFF);
	}

	attributes_[handle] = attr;

	update_service_end_handle(service_handle, handle);

	LOG(Info, "Added include at handle " << handle);
	return handle;
}

uint16_t BLEAttributeDatabase::add_characteristic(uint16_t service_handle,
                                                  const UUID& uuid,
                                                  uint8_t properties,
                                                  uint8_t permissions)
{
	ENTER();

	// Allocate handles for:
	// 1. Characteristic declaration
	// 2. Characteristic value
	uint16_t decl_handle = allocate_handle();
	uint16_t value_handle = allocate_handle();

	if (decl_handle == 0 || value_handle == 0) return 0;

	// 1. Add characteristic declaration
	Attribute decl_attr;
	decl_attr.handle = decl_handle;
	decl_attr.type = AttributeType::CHARACTERISTIC;
	decl_attr.uuid = UUID_CHARACTERISTIC;
	decl_attr.permissions = ATT_PERM_READ;
	decl_attr.properties = properties;
	decl_attr.value_handle = value_handle;

	// Value format: properties (1 byte) + value_handle (2 bytes) + UUID
	decl_attr.value.push_back(properties);
	decl_attr.value.push_back(value_handle & 0xFF);
	decl_attr.value.push_back((value_handle >> 8) & 0xFF);

	if (uuid.type == BT_UUID16) {
		uint16_t val = uuid.value.u16;
		decl_attr.value.push_back(val & 0xFF);
		decl_attr.value.push_back((val >> 8) & 0xFF);
	} else {
		// 128-bit UUID - ATT requires little-endian, which is how we store it
		size_t start_pos = decl_attr.value.size();
		decl_attr.value.resize(start_pos + 16);
		memcpy(&decl_attr.value[start_pos], uuid.value.u128.data, 16);
	}

	attributes_[decl_handle] = decl_attr;

	// 2. Add characteristic value
	Attribute value_attr;
	value_attr.handle = value_handle;
	value_attr.type = AttributeType::CHARACTERISTIC_VALUE;
	value_attr.uuid = uuid;
	value_attr.permissions = permissions;
	value_attr.properties = properties;

	attributes_[value_handle] = value_attr;

	update_service_end_handle(service_handle, value_handle);

	// 3. Auto-add CCCD if notify or indicate is enabled
	if (properties & (GATT_CHR_PROP_NOTIFY | GATT_CHR_PROP_INDICATE)) {
		uint16_t cccd_handle = add_cccd(value_handle);
		if (cccd_handle) {
			update_service_end_handle(service_handle, cccd_handle);
		}
	}

	LOG(Info, "Added characteristic " << uuid.str() << " (decl=" << decl_handle
	    << ", value=" << value_handle << ")");

	return decl_handle;
}

uint16_t BLEAttributeDatabase::add_descriptor(uint16_t char_handle,
                                              const UUID& uuid,
                                              uint8_t permissions)
{
	ENTER();

	uint16_t handle = allocate_handle();
	if (handle == 0) return 0;

	Attribute attr;
	attr.handle = handle;
	attr.type = AttributeType::DESCRIPTOR;
	attr.uuid = uuid;
	attr.permissions = permissions;

	attributes_[handle] = attr;

	// Find the service this belongs to and update end handle
	// (char_handle should be a characteristic value, so search backwards for service)
	for (auto rit = services_.rbegin(); rit != services_.rend(); ++rit) {
		if (char_handle >= rit->start_handle && char_handle <= rit->end_handle) {
			rit->end_handle = handle;
			// Update the service attribute's end_group_handle
			auto svc_attr = get_attribute(rit->start_handle);
			if (svc_attr) {
				svc_attr->end_group_handle = handle;
			}
			break;
		}
	}

	LOG(Info, "Added descriptor " << uuid.str() << " at handle " << handle);
	return handle;
}

uint16_t BLEAttributeDatabase::add_cccd(uint16_t char_value_handle)
{
	ENTER();

	uint16_t handle = add_descriptor(char_value_handle, UUID_CCCD,
	                                 ATT_PERM_READ | ATT_PERM_WRITE);

	if (handle) {
		// Initialize CCCD value to 0x0000 (notifications and indications disabled)
		auto attr = get_attribute(handle);
		if (attr) {
			attr->value = {0x00, 0x00};
		}
		LOG(Debug, "Auto-added CCCD at handle " << handle
		    << " for characteristic " << char_value_handle);
	}

	return handle;
}

void BLEAttributeDatabase::update_service_end_handle(uint16_t service_handle,
                                                     uint16_t last_handle)
{
	// Update the service info
	for (auto& svc : services_) {
		if (svc.start_handle == service_handle) {
			svc.end_handle = last_handle;
			break;
		}
	}

	// Update the service attribute itself
	auto attr = get_attribute(service_handle);
	if (attr) {
		attr->end_group_handle = last_handle;
	}
}

int BLEAttributeDatabase::register_services(const std::vector<GATTServiceDef>& services)
{
	ENTER();

	for (const auto& svc_def : services) {
		// Add service
		uint16_t svc_handle;
		if (svc_def.type == GATTServiceType::PRIMARY) {
			svc_handle = add_primary_service(svc_def.uuid);
		} else {
			svc_handle = add_secondary_service(svc_def.uuid);
		}

		if (svc_handle == 0) {
			LOG(Error, "Failed to add service " << svc_def.uuid.str());
			return -1;
		}

		// Fill in handle pointer if provided
		if (svc_def.handle_ptr) {
			*svc_def.handle_ptr = svc_handle;
		}

		// Add included services
		for (uint16_t inc_handle : svc_def.included_services) {
			add_include(svc_handle, inc_handle);
		}

		// Add characteristics
		for (const auto& char_def : svc_def.characteristics) {
			uint8_t properties = flags_to_properties(char_def.flags);
			uint8_t permissions = flags_to_permissions(char_def.flags);

			uint16_t char_decl_handle = add_characteristic(
				svc_handle,
				char_def.uuid,
				properties,
				permissions
			);

			if (char_decl_handle == 0) {
				LOG(Error, "Failed to add characteristic " << char_def.uuid.str());
				return -1;
			}

			// The value handle is always declaration handle + 1
			uint16_t char_value_handle = char_decl_handle + 1;

			// Fill in handle pointer if provided
			if (char_def.val_handle_ptr) {
				*char_def.val_handle_ptr = char_value_handle;
			}

			// Set access callback
			if (char_def.access_cb) {
				auto value_attr = get_attribute(char_value_handle);
				if (value_attr) {
					value_attr->read_cb = [char_def](uint16_t conn_handle, uint16_t offset,
					                                 std::vector<uint8_t>& out_data) -> int {
						return char_def.access_cb(conn_handle, ATTAccessOp::READ_CHR, offset, out_data);
					};

					value_attr->write_cb = [char_def](uint16_t conn_handle,
					                                  const std::vector<uint8_t>& data) -> int {
						std::vector<uint8_t> mutable_data = data;
						return char_def.access_cb(conn_handle, ATTAccessOp::WRITE_CHR, 0, mutable_data);
					};
				}
			}

			// Add descriptors
			for (const auto& dsc_def : char_def.descriptors) {
				uint16_t dsc_handle = add_descriptor(char_value_handle,
				                                     dsc_def.uuid,
				                                     dsc_def.permissions);

				if (dsc_handle == 0) {
					LOG(Error, "Failed to add descriptor " << dsc_def.uuid.str());
					return -1;
				}

				// Fill in handle pointer if provided
				if (dsc_def.handle_ptr) {
					*dsc_def.handle_ptr = dsc_handle;
				}

				// Set access callback
				if (dsc_def.access_cb) {
					auto dsc_attr = get_attribute(dsc_handle);
					if (dsc_attr) {
						dsc_attr->read_cb = [dsc_def](uint16_t conn_handle, uint16_t offset,
						                             std::vector<uint8_t>& out_data) -> int {
							return dsc_def.access_cb(conn_handle, ATTAccessOp::READ_DSC, offset, out_data);
						};

						dsc_attr->write_cb = [dsc_def](uint16_t conn_handle,
						                              const std::vector<uint8_t>& data) -> int {
							std::vector<uint8_t> mutable_data = data;
							return dsc_def.access_cb(conn_handle, ATTAccessOp::WRITE_DSC, 0, mutable_data);
						};
					}
				}
			}
		}
	}

	LOG(Info, "Registered " << services.size() << " services, total attributes: " << attributes_.size());
	return 0;
}

Attribute* BLEAttributeDatabase::get_attribute(uint16_t handle)
{
	auto it = attributes_.find(handle);
	return (it != attributes_.end()) ? &it->second : nullptr;
}

const Attribute* BLEAttributeDatabase::get_attribute(uint16_t handle) const
{
	auto it = attributes_.find(handle);
	return (it != attributes_.end()) ? &it->second : nullptr;
}

std::vector<const Attribute*> BLEAttributeDatabase::find_by_type(
	uint16_t start_handle,
	uint16_t end_handle,
	const UUID& type) const
{
	ENTER();
	LOG(Debug, "Searching for type=" << type.str() << " in range [" << start_handle << ", " << end_handle << "]");
	LOG(Debug, "Total attributes in database: " << attributes_.size());

	std::vector<const Attribute*> results;

	for (const auto& pair : attributes_) {
		const auto& attr = pair.second;
		if (attr.handle >= start_handle && attr.handle <= end_handle) {
			LOG(Debug, "Checking handle " << attr.handle << " with UUID " << attr.uuid.str());
			if (attr.uuid == type) {
				LOG(Debug, "  -> MATCH!");
				results.push_back(&attr);
			}
		}
	}

	LOG(Debug, "Found " << results.size() << " matching attributes");
	return results;
}

std::vector<const Attribute*> BLEAttributeDatabase::find_by_type_value(
	uint16_t start_handle,
	uint16_t end_handle,
	const UUID& type,
	const std::vector<uint8_t>& value) const
{
	std::vector<const Attribute*> results;

	for (const auto& pair : attributes_) {
		const auto& attr = pair.second;
		if (attr.handle >= start_handle && attr.handle <= end_handle) {
			if (attr.uuid == type && attr.value == value) {
				results.push_back(&attr);
			}
		}
	}

	return results;
}

std::vector<const Attribute*> BLEAttributeDatabase::get_range(
	uint16_t start_handle,
	uint16_t end_handle) const
{
	std::vector<const Attribute*> results;

	for (const auto& pair : attributes_) {
		const auto& attr = pair.second;
		if (attr.handle >= start_handle && attr.handle <= end_handle) {
			results.push_back(&attr);
		}
	}

	return results;
}

void BLEAttributeDatabase::clear()
{
	attributes_.clear();
	services_.clear();
	next_handle_ = 1;
}

int BLEAttributeDatabase::set_characteristic_value(uint16_t char_value_handle,
                                                   const std::vector<uint8_t>& value)
{
	auto attr = get_attribute(char_value_handle);
	if (!attr) {
		LOG(Warning, "Characteristic value handle " << char_value_handle << " not found");
		return -1;
	}

	if (attr->type != AttributeType::CHARACTERISTIC_VALUE) {
		LOG(Warning, "Handle " << char_value_handle << " is not a characteristic value");
		return -1;
	}

	attr->value = value;
	return 0;
}

std::vector<uint8_t> BLEAttributeDatabase::get_characteristic_value(uint16_t char_value_handle) const
{
	auto attr = get_attribute(char_value_handle);
	if (!attr) {
		return {};
	}

	if (attr->type != AttributeType::CHARACTERISTIC_VALUE) {
		return {};
	}

	return attr->value;
}

int BLEAttributeDatabase::set_read_callback(uint16_t char_value_handle,
                                           std::function<int(uint16_t conn_handle, uint16_t offset,
                                                            std::vector<uint8_t>& out_data)> cb)
{
	auto attr = get_attribute(char_value_handle);
	if (!attr) return -1;

	attr->read_cb = cb;
	return 0;
}

int BLEAttributeDatabase::set_write_callback(uint16_t char_value_handle,
                                            std::function<int(uint16_t conn_handle,
                                                             const std::vector<uint8_t>& data)> cb)
{
	auto attr = get_attribute(char_value_handle);
	if (!attr) return -1;

	attr->write_cb = cb;
	return 0;
}

} // namespace BLEPP

#endif // BLEPP_SERVER_SUPPORT
