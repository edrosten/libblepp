# Client Transport Abstraction

## Overview

The client functionality of libblepp has been refactored to support multiple transport layers through a unified abstraction. This allows the library to work with both standard BlueZ (HCI/L2CAP) and NIMBLE-specific hardware (/dev/atbm_ioctl).

---

## Build Configuration

### Preprocessor Flags

Three main flags control the build:

1. **`BLEPP_BLUEZ_SUPPORT`** - Enable BlueZ transport (HCI/L2CAP)
   - Uses `bluetooth.h`, HCI sockets, L2CAP sockets
   - Standard Linux Bluetooth stack
   - Default for most systems

2. **`BLEPP_NIMBLE_SUPPORT`** - Enable NIMBLE transport (/dev/atbm_ioctl)
   - Uses NIMBLE-specific ioctl interface
   - For NIMBLE WiFi/BLE combo chips
   - Can be used with or without BlueZ

3. **`BLEPP_SERVER_SUPPORT`** - Enable server/peripheral mode
   - Optional feature
   - Requires at least one transport

### Build Configurations

**Configuration Matrix:**

| BlueZ | NIMBLE | Server | Result |
|-------|------|--------|--------|
| ON    | OFF  | OFF    | Client-only with BlueZ |
| ON    | OFF  | ON     | Client + Server with BlueZ |
| OFF   | ON   | OFF    | Client-only with NIMBLE |
| OFF   | ON   | ON     | Client + Server with NIMBLE |
| ON    | ON   | OFF    | Client with both (runtime select) |
| ON    | ON   | ON     | Full featured with both transports |
| OFF   | OFF  | any    | **COMPILE ERROR** - need at least one transport |

### Build Examples

#### BlueZ Only (Default)
```bash
# Makefile
make BLEPP_BLUEZ_SUPPORT=1

# CMake
cmake -DWITH_BLUEZ_SUPPORT=ON ..
make
```

#### NIMBLE Only
```bash
# Makefile
make BLEPP_NIMBLE_SUPPORT=1

# CMake
cmake -DWITH_NIMBLE_SUPPORT=ON ..
make
```

#### Both Transports
```bash
# Makefile
make BLEPP_BLUEZ_SUPPORT=1 BLEPP_NIMBLE_SUPPORT=1

# CMake
cmake -DWITH_BLUEZ_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=ON ..
make
```

#### Full Build (Both Transports + Server)
```bash
# Makefile
make BLEPP_BLUEZ_SUPPORT=1 BLEPP_NIMBLE_SUPPORT=1 BLEPP_SERVER_SUPPORT=1

# CMake
cmake -DWITH_BLUEZ_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=ON -DWITH_SERVER_SUPPORT=ON ..
make
```

---

## Architecture

### Transport Abstraction Layer

```
┌─────────────────────────────────────────────┐
│         User Application                    │
└──────────────────┬──────────────────────────┘
                   │
         ┌─────────┴─────────┐
         │                   │
    ┌────▼────┐       ┌──────▼──────┐
    │ Scanner │       │ GATT Client │
    └────┬────┘       └──────┬──────┘
         │                   │
         └─────────┬─────────┘
                   │
         ┌─────────▼─────────────┐
         │ BLEClientTransport    │  ◄── Abstract Interface
         │   (Pure Virtual)      │
         └─────────┬─────────────┘
                   │
      ┌────────────┴────────────┐
      │                         │
┌─────▼──────────┐    ┌─────────▼─────────┐
│ BlueZ          │    │ NIMBLE              │
│ Client         │    │ Client            │
│ Transport      │    │ Transport         │
└─────┬──────────┘    └─────────┬─────────┘
      │                         │
      │                         │
┌─────▼──────────┐    ┌─────────▼─────────┐
│ HCI + L2CAP    │    │ /dev/atbm_ioctl   │
│ Sockets        │    │ + SIGIO Events    │
└────────────────┘    └───────────────────┘
```

### Key Classes

#### `BLEClientTransport` (Abstract Interface)
- Pure virtual base class
- Defines scanning, connection, and data transfer operations
- Located in: `blepp/bleclienttransport.h`

#### `BlueZClientTransport`
- Implements BLEClientTransport using BlueZ
- Uses HCI for scanning, L2CAP for connections
- Gated by `#ifdef BLEPP_BLUEZ_SUPPORT`
- Located in: `blepp/bluez_client_transport.h` / `src/bluez_client_transport.cc`

#### `NIMBLEClientTransport`
- Implements BLEClientTransport using NIMBLE ioctl
- Uses /dev/atbm_ioctl for all operations
- Event-driven with SIGIO signal handling
- Gated by `#ifdef BLEPP_NIMBLE_SUPPORT`
- Located in: `blepp/atbm_client_transport.h` / `src/atbm_client_transport.cc`

---

## API

### Scanning

```cpp
#include <blepp/bleclienttransport.h>

// Create transport
BLEClientTransport* transport = create_client_transport();

// Configure scan
ScanParams params;
params.scan_type = ScanParams::ScanType::Active;
params.interval_ms = 100;
params.window_ms = 50;

// Start scanning
transport->start_scan(params);

// Get results
std::vector<AdvertisementData> ads;
while (running) {
    int count = transport->get_advertisements(ads, 1000);  // 1 sec timeout
    for (const auto& ad : ads) {
        std::cout << "Device: " << ad.address << " RSSI: " << (int)ad.rssi << std::endl;
    }
}

transport->stop_scan();
```

### Connecting

```cpp
// Connect to device
ClientConnectionParams conn_params;
conn_params.peer_address = "AA:BB:CC:DD:EE:FF";
conn_params.peer_address_type = 0;  // Public address

int fd = transport->connect(conn_params);
if (fd < 0) {
    // Error
}

// Send ATT data
uint8_t pdu[] = {0x02, 0x17, 0x00};  // MTU exchange request
transport->send(fd, pdu, sizeof(pdu));

// Receive response
uint8_t buf[512];
int len = transport->receive(fd, buf, sizeof(buf));

// Disconnect
transport->disconnect(fd);
```

---

## Migration Guide

### For Existing Code Using BlueZ Directly

**Before:**
```cpp
#include <blepp/blestatemachine.h>

BLEGATTStateMachine gatt;
gatt.connect_blocking("AA:BB:CC:DD:EE:FF");
```

**After:**
No changes needed! The BLEGATTStateMachine class has been updated internally to use the transport abstraction, but the public API remains the same for backward compatibility.

### For Code That Needs Both Transports

```cpp
#ifdef BLEPP_BLUEZ_SUPPORT
    transport = new BlueZClientTransport();
#elif defined(BLEPP_NIMBLE_SUPPORT)
    transport = new NIMBLEClientTransport();
#endif

// Or use factory:
transport = create_client_transport();  // Returns appropriate transport
```

---

## Files Modified/Created

### New Files

**Headers:**
- `blepp/bleclienttransport.h` - Abstract transport interface
- `blepp/bluez_client_transport.h` - BlueZ implementation
- `blepp/nimble_client_transport.h` - NIMBLE implementation

**Implementation:**
- `src/bluez_client_transport.cc` - BlueZ transport
- `src/nimble_client_transport.cc` - NIMBLE transport

### Modified Files

**Configuration:**
- `blepp/blepp_config.h` - Added BLUEZ_SUPPORT and NIMBLE_SUPPORT flags
- `CMakeLists.txt` - Added WITH_BLUEZ_SUPPORT and WITH_NIMBLE_SUPPORT options
- `Makefile.in` - Added conditional compilation for both transports

**Core Classes (Internal Changes Only):**
- `blepp/blestatemachine.h` - Uses BLEClientTransport internally
- `src/blestatemachine.cc` - Refactored to use transport
- `blepp/lescan.h` - Uses BLEClientTransport for scanning
- `src/lescan.cc` - Refactored scanner implementation

---

## Testing

### Test BlueZ Transport
```bash
make clean
make BLEPP_BLUEZ_SUPPORT=1 BLEPP_EXAMPLES=1
sudo ./examples/lescan_simple
```

### Test NIMBLE Transport
```bash
make clean
make BLEPP_NIMBLE_SUPPORT=1 BLEPP_EXAMPLES=1
sudo ./examples/lescan_simple
```

### Test Both
```bash
make clean
make BLEPP_BLUEZ_SUPPORT=1 BLEPP_NIMBLE_SUPPORT=1 BLEPP_EXAMPLES=1
# Will use default transport (BlueZ if available, NIMBLE otherwise)
sudo ./examples/lescan_simple
```

---

## Related Documentation

- `BUILD_OPTIONS.md` - Makefile build options
- `CMAKE_BUILD_GUIDE.md` - CMake build guide
- `CONDITIONAL_COMPILATION.md` - Preprocessor flags reference
- `NIMBLE_IOCTL_API.md` - NIMBLE ioctl interface specification
