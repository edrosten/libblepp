/*
 *
 *  libattgatt - Implementation of the Generic ATTribute Protocol
 *
 *  Copyright (C) 2013, 2014 Edward Rosten
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

#ifndef __INC_LIBATTGATT_PRETTY_PRINTERS_H
#define __INC_LIBATTGATT_PRETTY_PRINTERS_H

#include "lib/uuid.h"
#include <string>
#include <vector>
#include <cstdint>
std::string to_hex(const std::uint16_t& u);
std::string to_hex(const std::uint8_t& u);
std::string to_str(const std::uint8_t& u);
std::string to_str(const bt_uuid_t& uuid);
std::string to_hex(const std::uint8_t* d, int l);
std::string to_hex(pair<const std::uint8_t*, int> d);
std::string to_hex(const std::vector<std::uint8_t>& v);
std::string to_str(const std::uint8_t* d, int l);
std::string to_str(pair<const std::uint8_t*, int> d);
std::string to_str(pair<const std::uint8_t*, const std::uint8_t*> d);
std::string to_str(const std::vector<std::uint8_t>& v);
#endif
