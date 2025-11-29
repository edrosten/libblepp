# Building Nimble BLE Stack for libblepp

This document explains how to build the Apache Nimble BLE stack as a shared library for use with libblepp's Nimble transport.

## Overview

libblepp's Nimble transport requires the Apache Nimble BLE stack to be built as a shared library. This avoids bloating libblepp with statically linked Nimble code (which can add ~5MB to the library size).

## Prerequisites

- GCC or Clang compiler
- Make
- The Nimble source code (from ATBM project or Apache Nimble repository)

## Build Instructions

### Option 1: Build from ATBM Project

If you're using the ATBM Nimble sources:

```bash
cd /path/to/atbm-wifi/ble_host/nimble_v42

# Create build directory
mkdir -p build
cd build

# Create a simple Makefile for building libnimble.so
cat > Makefile << 'EOF'
CC = gcc
AR = ar
CFLAGS = -fPIC -O2 -g -Wall

# Source directories
NIMBLE_ROOT = ..
HOST_SRC = $(NIMBLE_ROOT)/nimble/host/src
SERVICES_GAP = $(NIMBLE_ROOT)/nimble/host/services/gap/src
SERVICES_GATT = $(NIMBLE_ROOT)/nimble/host/services/gatt/src
UTIL_SRC = $(NIMBLE_ROOT)/nimble/host/util/src
PORT_SRC = $(NIMBLE_ROOT)/porting/nimble/src
CORE_SRC = $(NIMBLE_ROOT)/nimble/src
CRYPTO_SRC = $(NIMBLE_ROOT)/ext/tinycrypt/src

# Include directories
INCLUDES = -I$(NIMBLE_ROOT)/nimble/include \
           -I$(NIMBLE_ROOT)/nimble/host/include \
           -I$(NIMBLE_ROOT)/nimble/host/services/gap/include \
           -I$(NIMBLE_ROOT)/nimble/host/services/gatt/include \
           -I$(NIMBLE_ROOT)/nimble/host/util/include \
           -I$(NIMBLE_ROOT)/porting/nimble/include \
           -I$(NIMBLE_ROOT)/ext/tinycrypt/include

# Collect all source files
SRCS = $(wildcard $(HOST_SRC)/*.c) \
       $(wildcard $(SERVICES_GAP)/*.c) \
       $(wildcard $(SERVICES_GATT)/*.c) \
       $(wildcard $(UTIL_SRC)/*.c) \
       $(wildcard $(CORE_SRC)/*.c) \
       $(wildcard $(CRYPTO_SRC)/*.c) \
       $(PORT_SRC)/endian.c \
       $(PORT_SRC)/mem.c \
       $(PORT_SRC)/nimble_port.c \
       $(PORT_SRC)/os_mbuf.c \
       $(PORT_SRC)/os_mempool.c \
       $(PORT_SRC)/os_msys_init.c

OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: libnimble.so

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

libnimble.so: $(OBJS)
	$(CC) -shared -o $@ $^
	@echo "Built libnimble.so successfully"

clean:
	rm -f $(OBJS) libnimble.so
EOF

# Build the library
make

# Verify it was built
ls -lh libnimble.so
```

### Option 2: Build from Apache Nimble Repository

If you're using the official Apache Nimble repository:

```bash
git clone https://github.com/apache/mynewt-nimble.git
cd mynewt-nimble

# Follow Apache Nimble's build instructions for your platform
# Typically involves using newt or cmake with specific configuration
```

## Installation

After building, you have two options:

### Option A: Use from build directory

Set the `NIMBLE_ROOT` environment variable or CMake/Make variable to point to your Nimble directory:

```bash
# For CMake
cmake -DWITH_NIMBLE_SUPPORT=ON -DNIMBLE_ROOT=/path/to/atbm-wifi/ble_host/nimble_v42 ..

# For Make
make BLEPP_NIMBLE_SUPPORT=1 NIMBLE_ROOT=/path/to/atbm-wifi/ble_host/nimble_v42
```

### Option B: Install system-wide

```bash
# Copy library
sudo cp build/libnimble.so /usr/local/lib/

# Copy headers
sudo mkdir -p /usr/local/include/nimble
sudo cp -r nimble/include/* /usr/local/include/nimble/
sudo cp -r nimble/host/include/* /usr/local/include/nimble/
# ... copy other necessary headers

# Update library cache
sudo ldconfig
```

## Verification

To verify the Nimble library is properly built and linkable:

```bash
# Check library dependencies
ldd build/libnimble.so

# Check exported symbols
nm -D build/libnimble.so | grep ble_gap

# Check size (should be much smaller than 5MB)
ls -lh build/libnimble.so
```

Expected size: ~500KB - 1MB (depending on optimization and debug symbols)

## Troubleshooting

### Library not found error

If you get an error like:
```
Nimble library not found. Please build Nimble as a shared library first.
```

Make sure:
1. The library was built successfully: `ls $NIMBLE_ROOT/build/libnimble.so`
2. The `NIMBLE_ROOT` path is correct
3. The library is in either `lib/` or `build/` subdirectory of `NIMBLE_ROOT`

### Runtime linking errors

If you get runtime errors about missing libraries:

```bash
# Check where the library is being found
ldd /path/to/libble++.so | grep nimble

# Add to library path if needed
export LD_LIBRARY_PATH=/path/to/nimble/build:$LD_LIBRARY_PATH
```

Or use the rpath option (already included in libblepp's Makefile).

## Notes

- The Nimble library must be built with `-fPIC` (Position Independent Code) for shared library support
- Thread support may be required depending on your Nimble port
- Some Nimble ports may require additional dependencies (e.g., FreeRTOS, Zephyr OS layers)
