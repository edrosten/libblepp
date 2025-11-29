# libble++ - Modern C++ Bluetooth Low Energy Library

A modern C++ implementation of Bluetooth Low Energy (BLE) functionality with support for both client (central) and server (peripheral) modes, without requiring the BlueZ D-Bus API.

## Features

### Core Functionality
- **BLE Central/Client Mode**
  - Scan for BLE devices
  - Connect to peripherals
  - Service discovery (GATT)
  - Read/write characteristics
  - Subscribe to notifications/indications
  - Full ATT protocol implementation

- **BLE Peripheral/Server Mode** *(optional)*
  - Create custom GATT services
  - Advertise services
  - Accept incoming connections
  - Handle read/write requests
  - Send notifications/indications
  - Attribute database management

### Transport Layer Abstraction
libblepp supports multiple transport layers for maximum hardware compatibility:

- **BlueZ Transport** (Linux standard)
  - Uses HCI sockets for scanning
  - Uses L2CAP sockets for connections
  - Works with any BlueZ-compatible adapter

- **ATBM/NimBLE Transport** (hardware-specific)
  - Direct ioctl interface (`/dev/atbm_ioctl`)
  - Optimized for Altobeam WiFi+BLE combo chips
  - Signal-based asynchronous event handling
  - Full HCI packet wrapping/unwrapping

### Design Philosophy
- Clean, modern C++11/14 with callbacks
- Extensively commented with references to Bluetooth 4.0+ specifications
- Direct socket access for `select()`, `poll()`, or blocking I/O
- No dependency on BlueZ D-Bus API
- Thread-safe transport implementations

## Quick Start

### Installation

#### Using CMake (Recommended)
```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

#### Using Autoconf
```bash
./configure
make
sudo make install
```

### Basic Scanning Example
```cpp
#include <blepp/lescan.h>
#include <blepp/bleclienttransport.h>

int main() {
    BLEPP::log_level = BLEPP::LogLevels::Info;

    // Create transport (auto-selects available transport)
    BLEPP::BLEClientTransport* transport = BLEPP::create_client_transport();
    if (!transport) {
        return 1;
    }

    // Create scanner
    BLEPP::BLEScanner scanner(transport);
    scanner.start();

    while (1) {
        std::vector<BLEPP::AdvertisingResponse> ads = scanner.get_advertisements();
        // Process advertisements...
    }

    delete transport;
}
```

### Basic Server Example
```cpp
#include <blepp/blegattserver.h>

int main() {
    BLEPP::BLEGATTServer server;

    // Add a service with a characteristic
    auto service = server.add_service(0x180F);  // Battery Service
    auto characteristic = service->add_characteristic(
        0x2A19,  // Battery Level
        BLEPP::ReadOnly
    );

    characteristic->set_read_callback([](auto conn) {
        return std::vector<uint8_t>{85};  // 85% battery
    });

    server.start_advertising("MyDevice");
    server.run();  // Event loop
}
```

## Build Options

### CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `WITH_SERVER_SUPPORT` | `OFF` | Enable BLE peripheral/server mode |
| `WITH_BLUEZ_SUPPORT` | `ON` | Enable BlueZ HCI/L2CAP transport |
| `WITH_NIMBLE_SUPPORT` | `OFF` | Enable ATBM/NimBLE ioctl transport |
| `WITH_EXAMPLES` | `OFF` | Build example programs |

### Build Configuration Examples

**Client-only (BlueZ):**
```bash
cmake ..
make
```

**Client + Server (BlueZ):**
```bash
cmake -DWITH_SERVER_SUPPORT=ON ..
make
```

**Client + Server + ATBM:**
```bash
cmake -DWITH_SERVER_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=ON ..
make
```

**Everything with examples:**
```bash
cmake -DWITH_SERVER_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=ON -DWITH_EXAMPLES=ON ..
make
```

### Makefile Build Options

```bash
# Client-only
make

# Client + Server
make BLEPP_SERVER_SUPPORT=1

# Client + Server + ATBM
make BLEPP_SERVER_SUPPORT=1 BLEPP_NIMBLE_SUPPORT=1 BLEPP_BLUEZ_SUPPORT=1
```

See [BUILD_OPTIONS.md](docs/BUILD_OPTIONS.md) for complete build configuration reference.

## Example Programs

Located in the `examples/` directory:

**Client/Central Examples:**
- **lescan_simple** - Minimal BLE scanning example using transport abstraction
- **lescan** - Full-featured scanner with BlueZ HCI (signal handling, duplicate filtering)
- **lescan_transport** - Scanner using transport abstraction layer
- **temperature** - Read and log temperature characteristic with notifications
- **read_device_name** - Simple example reading device name characteristic
- **write** - Write to a characteristic (demonstrates write operations)
- **bluetooth** - Comprehensive example with notifications, plotting, and non-blocking I/O
- **blelogger** - Data logging from custom BLE device

**Server/Peripheral Examples:**
- **gatt_server** - Complete GATT server with Battery Service, Device Info, and custom services *(requires server support)*

Build examples with CMake:
```bash
mkdir build && cd build
cmake -DWITH_EXAMPLES=ON ..
make
```

Build with server support:
```bash
cmake -DWITH_SERVER_SUPPORT=ON -DWITH_EXAMPLES=ON ..
make
```

Examples will be in `build/examples/`.

Run examples (most require root for BLE access):
```bash
# Scan for devices
sudo ./examples/lescan

# Connect and read device name
sudo ./examples/read_device_name AA:BB:CC:DD:EE:FF

# Run GATT server
sudo ./examples/gatt_server "My BLE Device"
```

## Documentation

- [BUILD_OPTIONS.md](docs/BUILD_OPTIONS.md) - Complete build configuration reference
- [CMAKE_BUILD_GUIDE.md](docs/CMAKE_BUILD_GUIDE.md) - CMake build system guide
- [CLIENT_TRANSPORT_ABSTRACTION.md](docs/CLIENT_TRANSPORT_ABSTRACTION.md) - Transport layer architecture
- [ATBM_IOCTL_API.md](docs/ATBM_IOCTL_API.md) - ATBM transport API reference

## Requirements

### Common Requirements
- C++11 or later compiler
- Linux kernel 3.4+ (for BLE support)

### BlueZ Transport (default)
- BlueZ 5.0 or later
- libbluetooth-dev (development headers)
- Root privileges or `CAP_NET_ADMIN` + `CAP_NET_RAW` capabilities

Install on Debian/Ubuntu:
```bash
sudo apt-get install libbluetooth-dev
```

### ATBM Transport (optional)
- Altobeam WiFi+BLE driver loaded
- `/dev/atbm_ioctl` device accessible
- Appropriate device permissions
- Apache NimBLE 4.2+ (bundled with driver)

## Architecture

### Transport Abstraction
```
┌─────────────────────────────────────┐
│      Application Code               │
└──────────────┬──────────────────────┘
               │
    ┌──────────┴──────────┐
    │                     │
┌───▼─────┐      ┌────────▼────────┐
│ Scanner │      │  GATT Client    │
└───┬─────┘      └────────┬────────┘
    │                     │
    └──────────┬──────────┘
               │
    ┌──────────▼─────────────┐
    │ BLEClientTransport     │  ◄─ Abstract Interface
    │  (Pure Virtual)        │
    └──────────┬─────────────┘
               │
      ┌────────┴────────┐
      │                 │
┌─────▼──────┐    ┌─────▼──────┐
│ BlueZ      │    │ ATBM       │
│ Transport  │    │ Transport  │
└─────┬──────┘    └─────┬──────┘
      │                 │
┌─────▼──────┐    ┌─────▼──────┐
│ HCI/L2CAP  │    │ /dev/atbm  │
│ Sockets    │    │ ioctl      │
└────────────┘    └────────────┘
```

## Using libblepp in Your Project

### CMake
```cmake
find_library(BLEPP_LIB ble++ REQUIRED)
find_path(BLEPP_INCLUDE blepp REQUIRED)

add_executable(my_app main.cpp)
target_include_directories(my_app PRIVATE ${BLEPP_INCLUDE})
target_link_libraries(my_app ${BLEPP_LIB} bluetooth pthread)
```

### pkg-config
```bash
g++ main.cpp $(pkg-config --cflags --libs libblepp) -o my_app
```

### Direct Linking
```bash
g++ main.cpp -lble++ -lbluetooth -lpthread -o my_app
```

## License

[License information - please verify in source]

## Contributing

Contributions are welcome! Please ensure:
- Code follows existing style conventions
- Changes include relevant documentation updates
- Build succeeds with all configuration options
- Examples compile and run correctly

## References

- Bluetooth Core Specification 4.0+
- BlueZ 5.x documentation
- Apache NimBLE documentation
- Linux kernel Bluetooth subsystem

## Version History

See git history for detailed changelog. Recent major updates include:
- ATBM/NimBLE transport support with ioctl interface
- Transport abstraction layer for multiple hardware backends
- Complete GATT server implementation
- CMake build system alongside autoconf
- Comprehensive documentation suite

## Support & Issues

For bugs, feature requests, or questions:
1. Check existing documentation in `docs/`
2. Search closed issues
3. Open a new issue with details about your environment

## Credits

Originally designed for BlueZ-based systems, now extended to support multiple transport layers for broader hardware compatibility.
