/*
 * Simple GATT Server Example
 *
 * This example demonstrates creating a simple BLE peripheral with:
 * - Battery Service (0x180F)
 * - Device Information Service (0x180A)
 * - A custom notification service
 *
 * Compile with server support:
 *   cmake -DWITH_SERVER_SUPPORT=ON -DWITH_EXAMPLES=ON ..
 *   make
 *
 * Run:
 *   sudo ./examples/gatt_server
 */

#ifdef BLEPP_SERVER_SUPPORT

#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <blepp/blegattserver.h>
#include <blepp/bleattributedb.h>

using namespace std;
using namespace BLEPP;

// Global server instance for signal handler
static BLEGATTServer* g_server = nullptr;

// Signal handler for clean shutdown
void signal_handler(int signum)
{
	cerr << "\nReceived signal " << signum << ", shutting down..." << endl;
	if (g_server) {
		g_server->stop();
	}
}

int main(int argc, char** argv)
{
	log_level = LogLevels::Info;

	// Device name (can be overridden by command line)
	string device_name = "LibBLE++ Example";
	if (argc > 1) {
		device_name = argv[1];
	}

	cout << "Creating BLE GATT Server: " << device_name << endl;

	// Create server instance
	BLEGATTServer server;
	g_server = &server;

	// Set up signal handlers for graceful shutdown
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	//
	// Add Battery Service (0x180F)
	//
	auto battery_service = server.add_service(UUID("180F"));

	// Battery Level characteristic (0x2A19) - Read + Notify
	auto battery_level_char = battery_service->add_characteristic(
		UUID("2A19"),
		CharacteristicProperty::Read | CharacteristicProperty::Notify
	);

	// Simulated battery level (will decrease over time)
	uint8_t battery_level = 100;

	// Read callback for battery level
	battery_level_char->set_read_callback([&battery_level](uint16_t conn_handle) {
		cout << "Battery level read by connection " << conn_handle
		     << ": " << (int)battery_level << "%" << endl;
		return vector<uint8_t>{battery_level};
	});

	//
	// Add Device Information Service (0x180A)
	//
	auto device_info_service = server.add_service(UUID("180A"));

	// Manufacturer Name String (0x2A29)
	auto manufacturer_char = device_info_service->add_characteristic(
		UUID("2A29"),
		CharacteristicProperty::Read
	);
	manufacturer_char->set_read_callback([](uint16_t conn_handle) {
		string manufacturer = "LibBLE++ Project";
		return vector<uint8_t>(manufacturer.begin(), manufacturer.end());
	});

	// Model Number String (0x2A24)
	auto model_char = device_info_service->add_characteristic(
		UUID("2A24"),
		CharacteristicProperty::Read
	);
	model_char->set_read_callback([](uint16_t conn_handle) {
		string model = "v1.0";
		return vector<uint8_t>(model.begin(), model.end());
	});

	//
	// Add Custom Service for demonstrating write operations
	// UUID: 12345678-1234-5678-1234-56789abcdef0
	//
	auto custom_service = server.add_service(
		UUID("12345678-1234-5678-1234-56789abcdef0")
	);

	// LED Control characteristic (write)
	// UUID: 12345678-1234-5678-1234-56789abcdef1
	auto led_control_char = custom_service->add_characteristic(
		UUID("12345678-1234-5678-1234-56789abcdef1"),
		CharacteristicProperty::Write | CharacteristicProperty::Read
	);

	uint8_t led_state = 0;  // 0 = off, 1 = on

	led_control_char->set_write_callback([&led_state](uint16_t conn_handle, const vector<uint8_t>& data) {
		if (!data.empty()) {
			led_state = data[0];
			cout << "LED state changed to: " << (led_state ? "ON" : "OFF")
			     << " by connection " << conn_handle << endl;
		}
	});

	led_control_char->set_read_callback([&led_state](uint16_t conn_handle) {
		return vector<uint8_t>{led_state};
	});

	// Counter characteristic (notify)
	// UUID: 12345678-1234-5678-1234-56789abcdef2
	auto counter_char = custom_service->add_characteristic(
		UUID("12345678-1234-5678-1234-56789abcdef2"),
		CharacteristicProperty::Read | CharacteristicProperty::Notify
	);

	uint32_t counter = 0;

	counter_char->set_read_callback([&counter](uint16_t conn_handle) {
		// Return counter as 4-byte little-endian value
		return vector<uint8_t>{
			static_cast<uint8_t>(counter & 0xFF),
			static_cast<uint8_t>((counter >> 8) & 0xFF),
			static_cast<uint8_t>((counter >> 16) & 0xFF),
			static_cast<uint8_t>((counter >> 24) & 0xFF)
		};
	});

	//
	// Set up connection callbacks
	//
	server.on_connected = [](const ConnectionParams& params) {
		cout << "Device connected: " << params.peer_address
		     << " (handle: " << params.connection_handle << ")" << endl;
	};

	server.on_disconnected = [](uint16_t conn_handle, uint8_t reason) {
		cout << "Device disconnected (handle: " << conn_handle
		     << ", reason: " << (int)reason << ")" << endl;
	};

	//
	// Configure advertising
	//
	AdvertisingParams adv_params;
	adv_params.device_name = device_name;
	adv_params.service_uuids = {
		UUID("180F"),  // Battery Service
		UUID("180A"),  // Device Information
		UUID("12345678-1234-5678-1234-56789abcdef0")  // Custom Service
	};
	adv_params.connectable = true;
	adv_params.interval_min_ms = 100;
	adv_params.interval_max_ms = 200;

	//
	// Start advertising
	//
	cout << "Starting advertising as: " << device_name << endl;
	if (server.start_advertising(adv_params) < 0) {
		cerr << "Failed to start advertising" << endl;
		return 1;
	}

	cout << "Server running. Press Ctrl+C to stop." << endl;
	cout << "\nServices available:" << endl;
	cout << "  - Battery Service (0x180F)" << endl;
	cout << "  - Device Information (0x180A)" << endl;
	cout << "  - Custom Service (12345678-1234-5678-1234-56789abcdef0)" << endl;
	cout << "    - LED Control (write 0/1 to turn off/on)" << endl;
	cout << "    - Counter (read or subscribe for notifications)" << endl;
	cout << endl;

	//
	// Main event loop with periodic tasks
	//
	auto last_update = chrono::steady_clock::now();
	auto last_battery_update = chrono::steady_clock::now();

	while (server.is_running()) {
		auto now = chrono::steady_clock::now();

		// Update counter every second and send notifications
		if (chrono::duration_cast<chrono::seconds>(now - last_update).count() >= 1) {
			counter++;

			// Send notification to all subscribed clients
			vector<uint8_t> counter_data{
				static_cast<uint8_t>(counter & 0xFF),
				static_cast<uint8_t>((counter >> 8) & 0xFF),
				static_cast<uint8_t>((counter >> 16) & 0xFF),
				static_cast<uint8_t>((counter >> 24) & 0xFF)
			};
			counter_char->notify(counter_data);

			last_update = now;
		}

		// Decrease battery level every 10 seconds
		if (chrono::duration_cast<chrono::seconds>(now - last_battery_update).count() >= 10) {
			if (battery_level > 0) {
				battery_level -= 5;
				if (battery_level > 100) battery_level = 0;  // Handle underflow

				cout << "Battery level decreased to " << (int)battery_level << "%" << endl;

				// Send notification to subscribed clients
				battery_level_char->notify(vector<uint8_t>{battery_level});
			}
			last_battery_update = now;
		}

		// Process events and sleep briefly
		server.process_events(100);  // 100ms timeout
	}

	cout << "Server stopped." << endl;
	g_server = nullptr;

	return 0;
}

#else // !BLEPP_SERVER_SUPPORT

#include <iostream>
int main() {
	std::cerr << "This example requires server support." << std::endl;
	std::cerr << "Build with: cmake -DWITH_SERVER_SUPPORT=ON -DWITH_EXAMPLES=ON .." << std::endl;
	return 1;
}

#endif // BLEPP_SERVER_SUPPORT
