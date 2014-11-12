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
