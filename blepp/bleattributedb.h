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

#ifndef __INC_BLEPP_ATTRIBUTEDB_H
#define __INC_BLEPP_ATTRIBUTEDB_H

#include <blepp/blepp_config.h>

#ifdef BLEPP_SERVER_SUPPORT

#include <blepp/blestatemachine.h>
#include <blepp/att.h>
#include <cstdint>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace BLEPP
{
	// Forward declarations
	struct GATTServiceDef;
	struct GATTCharacteristicDef;
	struct GATTDescriptorDef;

	/// ATT Access operations
	enum class ATTAccessOp : uint8_t
	{
		READ_CHR = 0,      ///< Read characteristic value
		WRITE_CHR = 1,     ///< Write characteristic value
		READ_DSC = 2,      ///< Read descriptor value
		WRITE_DSC = 3      ///< Write descriptor value
	};

	/// ATT Attribute permissions
	enum ATTPermissions : uint8_t
	{
		ATT_PERM_NONE = 0x00,
		ATT_PERM_READ = 0x01,
		ATT_PERM_WRITE = 0x02,
		ATT_PERM_READ_ENCRYPT = 0x04,
		ATT_PERM_WRITE_ENCRYPT = 0x08,
		ATT_PERM_READ_AUTHEN = 0x10,
		ATT_PERM_WRITE_AUTHEN = 0x20,
		ATT_PERM_READ_AUTHOR = 0x40,
		ATT_PERM_WRITE_AUTHOR = 0x80
	};

	/// GATT Characteristic properties (from Bluetooth spec)
	enum GATTCharProperties : uint8_t
	{
		GATT_CHR_PROP_BROADCAST = 0x01,
		GATT_CHR_PROP_READ = 0x02,
		GATT_CHR_PROP_WRITE_NO_RSP = 0x04,
		GATT_CHR_PROP_WRITE = 0x08,
		GATT_CHR_PROP_NOTIFY = 0x10,
		GATT_CHR_PROP_INDICATE = 0x20,
		GATT_CHR_PROP_AUTH_WRITE = 0x40,
		GATT_CHR_PROP_EXTENDED = 0x80
	};

	/// GATT Characteristic flags (extended properties)
	enum GATTCharFlags : uint16_t
	{
		GATT_CHR_F_BROADCAST = 0x0001,
		GATT_CHR_F_READ = 0x0002,
		GATT_CHR_F_WRITE_NO_RSP = 0x0004,
		GATT_CHR_F_WRITE = 0x0008,
		GATT_CHR_F_NOTIFY = 0x0010,
		GATT_CHR_F_INDICATE = 0x0020,
		GATT_CHR_F_AUTH_SIGN_WRITE = 0x0040,
		GATT_CHR_F_RELIABLE_WRITE = 0x0080,
		GATT_CHR_F_AUX_WRITE = 0x0100,
		GATT_CHR_F_READ_ENC = 0x0200,
		GATT_CHR_F_READ_AUTHEN = 0x0400,
		GATT_CHR_F_READ_AUTHOR = 0x0800,
		GATT_CHR_F_WRITE_ENC = 0x1000,
		GATT_CHR_F_WRITE_AUTHEN = 0x2000,
		GATT_CHR_F_WRITE_AUTHOR = 0x4000
	};

	/// Attribute types
	enum class AttributeType : uint8_t
	{
		PRIMARY_SERVICE,      ///< Primary service declaration
		SECONDARY_SERVICE,    ///< Secondary service declaration
		INCLUDE,              ///< Include declaration
		CHARACTERISTIC,       ///< Characteristic declaration
		CHARACTERISTIC_VALUE, ///< Characteristic value
		DESCRIPTOR            ///< Descriptor (including CCCD)
	};

	/// A single attribute in the database
	struct Attribute
	{
		uint16_t handle;
		AttributeType type;
		UUID uuid;
		uint8_t permissions;
		std::vector<uint8_t> value;

		// For characteristic declarations
		uint8_t properties;       // GATT characteristic properties
		uint16_t value_handle;    // Points to the characteristic value handle

		// For service declarations
		uint16_t end_group_handle; // Last handle in service group

		// Callbacks for dynamic values
		std::function<int(uint16_t conn_handle, uint16_t offset,
		                 std::vector<uint8_t>& out_data)> read_cb;
		std::function<int(uint16_t conn_handle, const std::vector<uint8_t>& data)> write_cb;

		Attribute()
			: handle(0)
			, type(AttributeType::DESCRIPTOR)
			, permissions(0)
			, properties(0)
			, value_handle(0)
			, end_group_handle(0xFFFF)
		{}
	};

	/// GATT Attribute Database
	/// Manages all services, characteristics, and descriptors
	class BLEAttributeDatabase
	{
	public:
		BLEAttributeDatabase();
		~BLEAttributeDatabase();

		/// Register services from definition array
		/// @param services Array of service definitions (NimBLE-compatible)
		/// @return 0 on success, negative error code on failure
		int register_services(const std::vector<GATTServiceDef>& services);

		/// Add a primary service
		/// @param uuid Service UUID
		/// @return Service handle, or 0 on error
		uint16_t add_primary_service(const UUID& uuid);

		/// Add a secondary service
		/// @param uuid Service UUID
		/// @return Service handle, or 0 on error
		uint16_t add_secondary_service(const UUID& uuid);

		/// Add an include declaration
		/// @param service_handle Handle of the service containing the include
		/// @param included_service_handle Handle of the included service
		/// @return Include handle, or 0 on error
		uint16_t add_include(uint16_t service_handle, uint16_t included_service_handle);

		/// Add a characteristic
		/// @param service_handle Service handle
		/// @param uuid Characteristic UUID
		/// @param properties Characteristic properties (read, write, notify, etc.)
		/// @param permissions Attribute permissions
		/// @return Characteristic declaration handle, or 0 on error
		uint16_t add_characteristic(uint16_t service_handle,
		                            const UUID& uuid,
		                            uint8_t properties,
		                            uint8_t permissions);

		/// Add a descriptor
		/// @param char_handle Characteristic handle
		/// @param uuid Descriptor UUID
		/// @param permissions Attribute permissions
		/// @return Descriptor handle, or 0 on error
		uint16_t add_descriptor(uint16_t char_handle,
		                        const UUID& uuid,
		                        uint8_t permissions);

		/// Get attribute by handle
		/// @param handle Attribute handle
		/// @return Pointer to attribute, or nullptr if not found
		Attribute* get_attribute(uint16_t handle);
		const Attribute* get_attribute(uint16_t handle) const;

		/// Find attributes by type
		/// @param start_handle Start of search range
		/// @param end_handle End of search range
		/// @param type Attribute type to search for
		/// @return Vector of matching attributes
		std::vector<const Attribute*> find_by_type(uint16_t start_handle,
		                                           uint16_t end_handle,
		                                           const UUID& type) const;

		/// Find attributes by type and value
		/// @param start_handle Start of search range
		/// @param end_handle End of search range
		/// @param type Attribute type
		/// @param value Value to match
		/// @return Vector of matching attributes
		std::vector<const Attribute*> find_by_type_value(uint16_t start_handle,
		                                                 uint16_t end_handle,
		                                                 const UUID& type,
		                                                 const std::vector<uint8_t>& value) const;

		/// Find attributes in range
		/// @param start_handle Start handle (inclusive)
		/// @param end_handle End handle (inclusive)
		/// @return Vector of attributes in range
		std::vector<const Attribute*> get_range(uint16_t start_handle,
		                                        uint16_t end_handle) const;

		/// Get next handle value
		uint16_t get_next_handle() const { return next_handle_; }

		/// Get total number of attributes
		size_t size() const { return attributes_.size(); }

		/// Clear all attributes
		void clear();

		/// Set characteristic value
		/// @param char_value_handle Characteristic value handle
		/// @param value New value
		/// @return 0 on success, negative on error
		int set_characteristic_value(uint16_t char_value_handle,
		                             const std::vector<uint8_t>& value);

		/// Get characteristic value
		/// @param char_value_handle Characteristic value handle
		/// @return Value, or empty vector if not found
		std::vector<uint8_t> get_characteristic_value(uint16_t char_value_handle) const;

		/// Set read callback for a characteristic value
		int set_read_callback(uint16_t char_value_handle,
		                     std::function<int(uint16_t conn_handle, uint16_t offset,
		                                      std::vector<uint8_t>& out_data)> cb);

		/// Set write callback for a characteristic value
		int set_write_callback(uint16_t char_value_handle,
		                      std::function<int(uint16_t conn_handle,
		                                       const std::vector<uint8_t>& data)> cb);

	private:
		std::map<uint16_t, Attribute> attributes_;
		uint16_t next_handle_;

		// Service handle tracking for end_group_handle updates
		struct ServiceInfo {
			uint16_t start_handle;
			uint16_t end_handle;
		};
		std::vector<ServiceInfo> services_;

		/// Allocate a new handle
		uint16_t allocate_handle();

		/// Update service end group handle
		void update_service_end_handle(uint16_t service_handle, uint16_t last_handle);

		/// Get properties byte from flags
		static uint8_t flags_to_properties(uint16_t flags);

		/// Convert flags to permissions
		static uint8_t flags_to_permissions(uint16_t flags);

		/// Add CCCD (Client Characteristic Configuration Descriptor) if needed
		/// Called automatically when characteristic has notify or indicate
		uint16_t add_cccd(uint16_t char_value_handle);
	};

} // namespace BLEPP

#endif // BLEPP_SERVER_SUPPORT
#endif // __INC_BLEPP_ATTRIBUTEDB_H
