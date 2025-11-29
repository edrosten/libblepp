# Build Options Reference

## Preprocessor Flags

### `BLEPP_SERVER_SUPPORT`

Enables BLE GATT Server functionality.

**Default:** Not defined (disabled)

**Effect:**
- Compiles `src/bluez_transport.cc` or `src/nimble_transport.cc`
- Adds server-related headers to installation
- Adds ~50-80 KB to library size

**Usage:**
```bash
./configure && make BLEPP_SERVER_SUPPORT=1
```
---

### `BLEPP_NIMBLE_SUPPORT`

Enables NIMBLE ioctl transport support.

**Default:** Not defined (disabled)

**Effect:**
- Compiles `src/nimble_client_transport.cc`
- Adds NIMBLE-specific headers

**Usage:**
```bash
./configure && make BLEPP_SERVER_SUPPORT=1 BLEPP_NIMBLE_SUPPORT=1
```

### `BLEPP_BLUEZ_SUPPORT`

Enables BlueZ HCI/L2CAP transport support

**Default:** Not defined (disabled)

**Effect:**
- Compiles `src/bluez_client_transport.cc`

**Usage:**
```bash
./configure && make BLEPP_SERVER_SUPPORT=1 BLEPP_BLUEZ_SUPPORT=1

---
---

## Build Configurations

### Client-Only (Default)

**Size:** Baseline (~150 KB)

**Compile:**
```bash
./configure
make
```

**Features:**
- ✅ BLE Central/Client mode
- ✅ Service discovery
- ✅ Read/Write characteristics
- ✅ Receive notifications
- ✅ BLE Peripheral/Server mode
- ✅ Advertising
- ✅ Accept connections

---

### Client + Server (BlueZ)

**Size:** ~200-230 KB (+50-80 KB)

**Compile:**
```bash
./configure
make BLEPP_SERVER_SUPPORT=1
```

**Features:**
- ✅ All client features
- ✅ BLE Peripheral/Server mode
- ✅ Advertising
- ✅ Accept connections
- ✅ Handle read/write requests
- ✅ Send notifications/indications
- ✅ BlueZ HCI/L2CAP transport
- ✅ NIMBLE transport

---

### Client + Server + NIMBLE

**Size:** ~220-360 KB (+70-110 KB)

**Compile:**
```bash
./configure
make BLEPP_SERVER_SUPPORT=1 BLEPP_NIMBLE_SUPPORT=1
```

**Features:**
- ✅ All client + server features
- ✅ NIMBLE ioctl transport
- ✅ Hardware-specific optimizations

---

## Makefile Variables

### Setting Variables

**Command line:**
```bash
make BLEPP_SERVER_SUPPORT=1 BLEPP_NIMBLE_SUPPORT=1
```

**Environment:**
```bash
export BLEPP_SERVER_SUPPORT=1
export BLEPP_NIMBLE_SUPPORT=1
make
```

**Makefile override:**
```makefile
# At top of Makefile or Makefile.local
BLEPP_SERVER_SUPPORT = 1
BLEPP_NIMBLE_SUPPORT = 1
```
---

**Build:**
```bash
cmake -DWITH_SERVER_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=OFF ..
make
```

---

## Verification

### Check Compiled Features

```bash
# Check for server symbols
nm -D libble++.so | grep -i "bluez\|transport"

# Check for NIMBLE symbols
nm -D libble++.so | grep -i "NIMBLE"
```

### Runtime Check

```cpp
#ifdef BLEPP_SERVER_SUPPORT
    std::cout << "Server support: ENABLED" << std::endl;
#else
    std::cout << "Server support: DISABLED" << std::endl;
#endif

#ifdef BLEPP_NIMBLE_SUPPORT
    std::cout << "NIMBLE support: ENABLED" << std::endl;
#else
    std::cout << "NIMBLE support: DISABLED" << std::endl;
#endif

#ifdef BLEPP_BLUEZ_SUPPORT
    std::cout << "BLUEZ support: ENABLED" << std::endl;
#else
    std::cout << "BLUEZ support: DISABLED" << std::endl;
#endif
```

---

## Dependencies

### BlueZ
- `libbluetooth.so` (BlueZ 5.0+)
- Root privileges or `CAP_NET_ADMIN` + `CAP_NET_RAW` capabilities

### NIMBLE
- NIMBLE driver loaded
- `/dev/NIMBLE_ioctl` device accessible
- Appropriate permissions on ioctl device

---
---

## Troubleshooting

### "undefined reference to BlueZTransport"

**Cause:** Trying to use BlueZ features without defining `BLEPP_BLUEZ_SUPPORT`

**Fix:**
```bash
g++ -DBLEPP_SERVER_SUPPORT your_code.cpp -lblepp
```
### "/dev/NIMBLE_ioctl: No such file or directory"

**Cause:** NIMBLE driver not loaded

**Fix:**
```bash
# Load NIMBLE driver
sudo modprobe NIMBLE_wifi_ble

# Check device
ls -l /dev/NIMBLE_ioctl

Where NIMBLE is your drivers slug ('atbm' for Altobeam)
```
