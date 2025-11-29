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

#ifndef __INC_BLEPP_ADVERTISING_H
#define __INC_BLEPP_ADVERTISING_H

#include <blepp/lescan.h>
#include <vector>
#include <cstdint>

namespace BLEPP
{

/// Parse a BLE HCI advertisement packet into structured AdvertisingResponse
/// This is transport-agnostic and can be used by both BlueZ and ATBM implementations
/// @param packet Raw HCI event packet data
/// @return Vector of parsed advertising responses (may contain multiple advertisements)
std::vector<AdvertisingResponse> parse_advertisement_packet(const std::vector<uint8_t>& packet);

} // namespace BLEPP

#endif // __INC_BLEPP_ADVERTISING_H
