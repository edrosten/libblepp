/*
 *
 *  blepp - BLE++ Library Configuration
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

#ifndef __INC_BLEPP_CONFIG_H
#define __INC_BLEPP_CONFIG_H

//
// Build Configuration Flags
//

// ===== Transport Layer Support =====

// Enable BlueZ transport support (standard Linux Bluetooth stack)
// Uses bluetooth.h, HCI, and L2CAP sockets
// This is the default transport for most Linux systems
//
#ifndef BLEPP_BLUEZ_SUPPORT
// #define BLEPP_BLUEZ_SUPPORT
#endif

// Enable Nimble transport support
// Uses /dev/atbm_ioctl device for Nimble-based hardware
// Can be used with or without BlueZ support
//
#ifndef BLEPP_NIMBLE_SUPPORT
// #define BLEPP_NIMBLE_SUPPORT
#endif

// ===== Feature Support =====

// Enable BLE GATT Server functionality
// Define this to enable server/peripheral mode support
// If not defined, only BLE client/central mode is available
//
#ifndef BLEPP_SERVER_SUPPORT
// #define BLEPP_SERVER_SUPPORT
#endif

// ===== Validation =====

// Require at least one transport
#if !defined(BLEPP_BLUEZ_SUPPORT) && !defined(BLEPP_NIMBLE_SUPPORT)
  #error "At least one of BLEPP_BLUEZ_SUPPORT or BLEPP_NIMBLE_SUPPORT must be defined"
#endif

// Server support requires at least one transport (automatically satisfied by above check)
#ifdef BLEPP_SERVER_SUPPORT
  #if !defined(BLEPP_BLUEZ_SUPPORT) && !defined(BLEPP_NIMBLE_SUPPORT)
    #error "BLEPP_SERVER_SUPPORT requires at least one transport"
  #endif
#endif

#endif // __INC_BLEPP_CONFIG_H
