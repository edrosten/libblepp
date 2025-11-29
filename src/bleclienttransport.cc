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

#include <blepp/bleclienttransport.h>
#include <blepp/logging.h>

#ifdef BLEPP_SERVER_SUPPORT
#include <blepp/bletransport.h>
#endif

#ifdef BLEPP_BLUEZ_SUPPORT
#include <blepp/bluez_client_transport.h>
#ifdef BLEPP_SERVER_SUPPORT
#include <blepp/bluez_transport.h>
#endif
#endif

#ifdef BLEPP_NIMBLE_SUPPORT
#include <blepp/nimble_client_transport.h>
#ifdef BLEPP_SERVER_SUPPORT
#include <blepp/nimble_transport.h>
#endif
#endif

namespace BLEPP
{

BLEClientTransport* create_client_transport()
{
	ENTER();
	LOG(Info, "create_client_transport() called - selecting BLE client transport");

	// Prefer BlueZ if available and working, fall back to Nimble
#ifdef BLEPP_BLUEZ_SUPPORT
	{
		LOG(Debug, "Trying BlueZ client transport...");
		BlueZClientTransport* transport = new BlueZClientTransport();
		LOG(Debug, "BlueZ client transport created, checking availability...");
		if (transport->is_available()) {
			LOG(Info, "Using BlueZ client transport");
			return transport;
		}
		delete transport;
		LOG(Warning, "BlueZ transport not available, trying next option");
	}
#else
	LOG(Debug, "BlueZ support not compiled in (BLEPP_BLUEZ_SUPPORT not defined)");
#endif

#ifdef BLEPP_NIMBLE_SUPPORT
	{
		LOG(Debug, "Trying Nimble client transport...");
		NimbleClientTransport* transport = new NimbleClientTransport();
		LOG(Debug, "Nimble client transport created, checking availability...");
		if (transport->is_available()) {
			LOG(Info, "Using Nimble client transport");
			return transport;
		}
		delete transport;
		LOG(Warning, "Nimble transport not available");
	}
#else
	LOG(Debug, "Nimble support not compiled in (BLEPP_NIMBLE_SUPPORT not defined)");
#endif

	LOG(Error, "No BLE client transport available - all transports failed");
	return nullptr;
}

#ifdef BLEPP_BLUEZ_SUPPORT
BLEClientTransport* create_bluez_client_transport()
{
	ENTER();
	LOG(Info, "Creating BlueZ client transport");
	return new BlueZClientTransport();
}
#endif

#ifdef BLEPP_NIMBLE_SUPPORT
BLEClientTransport* create_nimble_client_transport()
{
	ENTER();
	LOG(Info, "Creating Nimble client transport");
	return new NimbleClientTransport();
}
#endif

// ===================================================================
// Server Transport Factory Functions
// ===================================================================

#ifdef BLEPP_SERVER_SUPPORT

BLETransport* create_server_transport()
{
	ENTER();

	// Prefer BlueZ if available and working, fall back to Nimble
#ifdef BLEPP_BLUEZ_SUPPORT
	{
		BlueZTransport* transport = new BlueZTransport();
		// BlueZ server doesn't have is_available(), so just check if it was created
		if (transport) {
			LOG(Info, "Using BlueZ server transport");
			return transport;
		}
		delete transport;
		LOG(Warning, "BlueZ server transport not available");
	}
#endif

#ifdef BLEPP_NIMBLE_SUPPORT
	{
		NimbleTransport* transport = new NimbleTransport();
		// Nimble server doesn't have is_available() either
		if (transport) {
			LOG(Info, "Using Nimble server transport");
			return transport;
		}
		delete transport;
		LOG(Warning, "Nimble server transport not available");
	}
#endif

	LOG(Error, "No BLE server transport available");
	return nullptr;
}

#ifdef BLEPP_BLUEZ_SUPPORT
BLETransport* create_bluez_server_transport()
{
	ENTER();
	LOG(Info, "Creating BlueZ server transport");
	return new BlueZTransport();
}
#endif

#ifdef BLEPP_NIMBLE_SUPPORT
BLETransport* create_nimble_server_transport()
{
	ENTER();
	LOG(Info, "Creating Nimble server transport");
	return new NimbleTransport();
}
#endif

#endif // BLEPP_SERVER_SUPPORT

} // namespace BLEPP
