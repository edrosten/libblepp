# CMake Build Guide

## Overview

libblepp now supports CMake for building the library with optional server and NIMBLE transport support. This guide covers all CMake build configurations and options.

---

## Quick Start

### Client-Only Build (Default)

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

### Server Support Build

```bash
mkdir build
cd build
cmake -DWITH_SERVER_SUPPORT=ON ..
make
sudo make install
```

### Server + NIMBLE Support Build

```bash
mkdir build
cd build
cmake -DWITH_SERVER_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=ON ..
make
sudo make install
```

---

## CMake Options

### `WITH_EXAMPLES`

**Description:** Build example applications

**Default:** `OFF`

**Usage:**
```bash
cmake -DWITH_EXAMPLES=ON ..
```

---

### `WITH_SERVER_SUPPORT`

**Description:** Enable BLE GATT server (peripheral) functionality

**Default:** `OFF`

**Effect:**
- Compiles `src/bluez_transport.cc`
- Compiles `src/bleattributedb.cc`
- Compiles `src/blegattserver.cc`
- Adds `-DBLEPP_SERVER_SUPPORT` to compiler flags
- Links with `pthread` library
- Installs server headers

**Usage:**
```bash
cmake -DWITH_SERVER_SUPPORT=ON ..
```

---

### `WITH_NIMBLE_SUPPORT`

**Description:** Enable NIMBLE ioctl transport

**Default:** `OFF`

**Requires:** `WITH_SERVER_SUPPORT=ON`

**Effect:**
- Compiles `src/nimble_transport.cc`
- Adds `-DBLEPP_NIMBLE_SUPPORT` to compiler flags
- Installs NIMBLE headers

**Usage:**
```bash
cmake -DWITH_SERVER_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=ON ..
```

---

## Build Configurations

### Configuration Matrix

| Configuration | Command | Headers | Objects | Size | Features |
|---------------|---------|---------|---------|------|----------|
| **Client-only** | `cmake ..` | 12 | 9 | ~150 KB | Client only |
| **+ Server** | `cmake -DWITH_SERVER_SUPPORT=ON ..` | 17 | 12 | ~230 KB | Client + Server (BlueZ) |
| **+ NIMBLE** | `cmake -DWITH_SERVER_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=ON ..` | 18 | 13 | ~260 KB | Client + Server + NIMBLE |
| **+ Examples** | `cmake -DWITH_EXAMPLES=ON ..` | - | - | - | Build examples too |

---

## Build Types

### Debug Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

**Features:**
- Debug symbols included
- No optimization
- Larger binaries
- Better for debugging

### Release Build (Default)

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

**Features:**
- Optimized for performance
- Smaller binaries
- No debug symbols
- Production-ready

---

## Installation Paths

### Default Install Locations

On Linux:
- Libraries: `/usr/local/lib/libble++.so`
- Headers: `/usr/local/include/blepp/`
- pkg-config: `/usr/local/lib/pkgconfig/libblepp.pc`

### Custom Install Prefix

```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/libblepp ..
make
sudo make install
```

Results in:
- `/opt/libblepp/lib/libble++.so`
- `/opt/libblepp/include/blepp/`

---

## Complete Build Examples

### Example 1: Client + Server + Examples

```bash
mkdir build && cd build
cmake -DWITH_SERVER_SUPPORT=ON -DWITH_EXAMPLES=ON ..
make -j$(nproc)
sudo make install
```

### Example 2: Debug Build with Server

```bash
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_SERVER_SUPPORT=ON ..
make
```

### Example 3: NIMBLE Build with Custom Prefix

```bash
mkdir build && cd build
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_SERVER_SUPPORT=ON \
  -DWITH_NIMBLE_SUPPORT=ON \
  -DCMAKE_INSTALL_PREFIX=$HOME/opt/libblepp \
  ..
make -j$(nproc)
make install  # No sudo needed for user prefix
```

### Example 4: Cross-Compilation for ARM

```bash
mkdir build-arm && cd build-arm
cmake \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-toolchain.cmake \
  -DWITH_SERVER_SUPPORT=ON \
  ..
make -j$(nproc)
```

---

## Using libblepp in Your CMake Project

### Method 1: Find Package (After Installation)

```cmake
cmake_minimum_required(VERSION 3.4)
project(my_ble_app)

# Find the installed library
find_library(BLEPP_LIB ble++ REQUIRED)
find_path(BLEPP_INCLUDE blepp REQUIRED)

# Check for server support
try_compile(HAS_SERVER_SUPPORT
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_server.cpp
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${BLEPP_INCLUDE}"
    LINK_LIBRARIES ${BLEPP_LIB})

if(HAS_SERVER_SUPPORT)
    add_definitions(-DBLEPP_SERVER_SUPPORT)
    message(STATUS "libblepp has server support")
endif()

add_executable(my_app main.cpp)
target_include_directories(my_app PRIVATE ${BLEPP_INCLUDE})
target_link_libraries(my_app ${BLEPP_LIB} bluetooth pthread)
```

### Method 2: pkg-config

```cmake
cmake_minimum_required(VERSION 3.4)
project(my_ble_app)

find_package(PkgConfig REQUIRED)
pkg_check_modules(BLEPP REQUIRED libblepp)

add_executable(my_app main.cpp)
target_include_directories(my_app PRIVATE ${BLEPP_INCLUDE_DIRS})
target_link_libraries(my_app ${BLEPP_LIBRARIES})
```

### Method 3: Add as Subdirectory

```cmake
cmake_minimum_required(VERSION 3.4)
project(my_ble_app)

# Add libblepp as subdirectory
set(WITH_SERVER_SUPPORT ON CACHE BOOL "" FORCE)
add_subdirectory(external/libblepp)

add_executable(my_app main.cpp)
target_link_libraries(my_app ble++)
```

---

## Verification

### Check Build Configuration

After building, verify what was compiled:

```bash
# Check for server symbols
nm -D libble++.so | grep -i "BLEGATTServer\|BlueZTransport"

# Check for NIMBLE symbols
nm -D libble++.so | grep -i "NIMBLETransport"

# List all headers that will be installed
find blepp -name "*.h" -type f
```

### Check Preprocessor Defines

Create a test file:

```cpp
// test_config.cpp
#include <blepp/blepp_config.h>
#include <iostream>

int main() {
#ifdef BLEPP_SERVER_SUPPORT
    std::cout << "Server support: YES" << std::endl;
#else
    std::cout << "Server support: NO" << std::endl;
#endif

#ifdef BLEPP_NIMBLE_SUPPORT
    std::cout << "NIMBLE support: YES" << std::endl;
#else
    std::cout << "NIMBLE support: NO" << std::endl;
#endif
    return 0;
}
```

Compile and run:
```bash
g++ -I/usr/local/include test_config.cpp -o test_config
./test_config
```

---

## Out-of-Source Builds

It's recommended to use out-of-source builds:

```bash
# Good: Build directory separate from source
mkdir build
cd build
cmake ..
make

# Bad: In-source build
cd libblepp
cmake .
make
```

Multiple build configurations:

```bash
# Client-only
mkdir build-client && cd build-client
cmake ..
make

# Server
mkdir build-server && cd build-server
cmake -DWITH_SERVER_SUPPORT=ON ..
make

# NIMBLE
mkdir build-nimble && cd build-nimble
cmake -DWITH_SERVER_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=ON ..
make
```

---

## Cleaning Builds

### Clean build artifacts

```bash
cd build
make clean
```

### Complete rebuild

```bash
rm -rf build
mkdir build
cd build
cmake ..
make
```

### Clean and rebuild specific target

```bash
cd build
make clean
make ble++
```

---

## Advanced CMake Options

### Compiler Selection

```bash
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ..
```

### Additional Compiler Flags

```bash
cmake -DCMAKE_CXX_FLAGS="-Wall -Wextra -O3" ..
```

### Verbose Build

```bash
make VERBOSE=1
```

Or:

```bash
cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..
make
```

### Parallel Build

```bash
make -j$(nproc)  # Use all CPU cores
make -j4         # Use 4 cores
```

---

## Troubleshooting

### "Bluez not found"

**Solution:**
```bash
# Ubuntu/Debian
sudo apt-get install libbluetooth-dev

# Fedora/RHEL
sudo dnf install bluez-libs-devel

# Arch
sudo pacman -S bluez-libs
```

**Solution:**
```bash
cmake -DWITH_SERVER_SUPPORT=ON -DWITH_NIMBLE_SUPPORT=ON ..
```

### Headers not found after install

**Cause:** Non-standard install prefix

**Solution:**
```bash
# Add to pkg-config path
export PKG_CONFIG_PATH=/opt/libblepp/lib/pkgconfig:$PKG_CONFIG_PATH

# Or use full path
g++ -I/opt/libblepp/include myapp.cpp -L/opt/libblepp/lib -lble++
```

## Comparison: Make vs CMake

| Feature | Makefile | CMake |
|---------|----------|-------|
| **Build Options** | `make BLEPP_SERVER_SUPPORT=1` | `cmake -DWITH_SERVER_SUPPORT=ON` |
| **Installation** | `make install` | `cmake .. && make install` |
| **Clean** | `make clean` | `make clean` or `rm -rf build` |
| **Parallel** | `make -j4` | `make -j4` |
| **Cross-compile** | Manual | Toolchain files |
| **IDE Support** | Limited | Excellent |
| **Windows** | Difficult | Native |

---

## IDE Integration

### CLion

1. Open libblepp directory in CLion
2. CMake options appear automatically
3. Set options in `File → Settings → Build → CMake`
4. Add profiles for different configurations

### Visual Studio Code

1. Install CMake Tools extension
2. Open libblepp directory
3. Select configure preset:
   - `Ctrl+Shift+P` → "CMake: Configure"
4. Build: `Ctrl+Shift+P` → "CMake: Build"

### Qt Creator

1. `File → Open File or Project`
2. Select `CMakeLists.txt`
3. Configure build settings
4. Build and run