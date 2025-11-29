# ATBM ioctl API Reference

## Overview

This document describes the ATBM BLE ioctl API extracted from the `ble_host` project. The ATBM hardware uses a custom `/dev/atbm_ioctl` device for BLE communication instead of standard BlueZ HCI.

---

## Device Node

**Path:** `/dev/atbm_ioctl`

**Major Number:** 121 (defined as `ATBM_IOCTL`)

---

## ioctl Commands

All ioctl commands use the base number `ATBM_IOCTL (121)`.

### 1. ATBM_AT_CMD_DIRECT

```c
#define ATBM_AT_CMD_DIRECT _IOW(ATBM_IOCTL, 0, unsigned int)
```

**Purpose:** Send AT commands directly to the device.

**Parameter Structure:**
```c
struct at_cmd_direct {
    uint32_t len;
    uint8_t cmd[1500];
};
```

**Usage:** Used for WiFi AT commands in the WiFi+BLE combined mode.

---

### 2. ATBM_BLE_SMART

```c
#define ATBM_BLE_SMART _IOW(ATBM_IOCTL, 1, unsigned int)
```

**Purpose:** Control BLE smart configuration mode.

**Parameter:** Same as `at_cmd_direct` structure.

**Known Commands:**
- `"ble_smt_start"` - Start BLE smart configuration

---

### 3. ATBM_BLE_COEXIST_START

```c
#define ATBM_BLE_COEXIST_START _IOW(ATBM_IOCTL, 0, unsigned int)
```

**Purpose:** Start BLE coexistence mode (initialize BLE subsystem).

**Parameter:** Pointer to ioctl data buffer (may be empty).

**When to call:** Before any BLE operations, during initialization.

---

### 4. ATBM_BLE_COEXIST_STOP

```c
#define ATBM_BLE_COEXIST_STOP _IOW(ATBM_IOCTL, 1, unsigned int)
```

**Purpose:** Stop BLE coexistence mode (shutdown BLE subsystem).

**Parameter:** Pointer to ioctl data buffer (may be empty).

**When to call:** During cleanup/shutdown.

---

### 5. ATBM_BLE_SET_ADV_DATA

```c
#define ATBM_BLE_SET_ADV_DATA _IOW(ATBM_IOCTL, 2, unsigned int)
```

**Purpose:** Set BLE advertising data.

**Parameter:** Pointer to advertising data buffer (format TBD from NimBLE code).

---

### 6. ATBM_BLE_ADV_RESP_MODE_START

```c
#define ATBM_BLE_ADV_RESP_MODE_START _IOW(ATBM_IOCTL, 3, unsigned int)
```

**Purpose:** Start advertising response mode.

**Parameter:** Configuration buffer.

---

### 7. ATBM_BLE_SET_RESP_DATA

```c
#define ATBM_BLE_SET_RESP_DATA _IOW(ATBM_IOCTL, 4, unsigned int)
```

**Purpose:** Set scan response data.

**Parameter:** Pointer to scan response data buffer.

---

### 8. ATBM_BLE_HIF_TXDATA

```c
#define ATBM_BLE_HIF_TXDATA _IOW(ATBM_IOCTL, 5, unsigned int)
```

**Purpose:** Transmit HCI data to the BLE controller.

**Parameter:** Pointer to HCI packet buffer.

**Buffer Format:**
```c
struct {
    uint16_t len;          // Total length of HCI packet
    uint8_t type;          // BLE_HCI_HIF_CMD (0x01) or BLE_HCI_HIF_ACL (0x02)
    uint8_t data[...];     // HCI packet data
};
```

**HCI Packet Types:**
- `BLE_HCI_HIF_CMD (0x01)` - HCI command
- `BLE_HCI_HIF_ACL (0x02)` - ACL data
- `BLE_HCI_HIF_EVT (0x04)` - Event (received only)

---

## Asynchronous Event Handling

The ATBM device uses **SIGIO signals** for asynchronous event notifications.

### Setup

1. Open `/dev/atbm_ioctl`
2. Set file owner: `fcntl(fd, F_SETOWN, getpid())`
3. Enable async mode: `fcntl(fd, F_SETFL, flags | FASYNC)`
4. Register signal handler: `signal(SIGIO, event_handler)`

### Reading Events

When SIGIO is received, read events using `read()`:

```c
struct status_async {
    uint8_t type;           // Event type (0-6)
    uint8_t driver_mode;    // Sub-type or reason
    uint8_t list_empty;     // 1 if no more events, 0 if more pending
    uint8_t event_buffer[MAX_SYNC_EVENT_BUFFER_LEN];  // 512 bytes
};
```

### Event Buffer Format

The `event_buffer` contains a WSM (WiFi-to-Host Message) header followed by HCI data:

```c
struct wsm_hdr {
    uint16_t len;   // Length of data following this header
    uint16_t id;    // Message type ID
};

// Followed by actual HCI packet data
```

**Message Type IDs:**
- `BLE_MSG_TYPE_ACK (value TBD)` - Acknowledgment/command complete
- `BLE_MSG_TYPE_EVT (value TBD)` - Asynchronous event

### Event Processing

```c
struct wsm_hdr *wsm = (struct wsm_hdr *)status.event_buffer;
uint8_t *hci_data = (uint8_t *)(wsm + 1);

if (wsm->id == BLE_MSG_TYPE_ACK) {
    // Process acknowledgment
    ble_hci_trans_hs_rx(1, hci_data, wsm->len);
} else if (wsm->id == BLE_MSG_TYPE_EVT) {
    // Process event
    ble_hci_trans_hs_rx(0, hci_data, wsm->len);
}
```

---

## HCI Packet Format

### Transmit (to controller)

**Command Packet:**
```
[length:2][type:1][opcode:2][param_len:1][params...]
```

**ACL Data Packet:**
```
[length:2][type:1][handle:2][data_len:2][data...]
```

### Receive (from controller)

**Event Packet:**
```
[type:1][event_code:1][param_len:1][params...]
```

**ACL Data Packet:**
```
[type:1][handle:2][data_len:2][data...]
```

---

## Initialization Sequence

1. Open `/dev/atbm_ioctl`
2. Configure async I/O (fcntl + signal)
3. Call `ATBM_BLE_COEXIST_START` ioctl
4. Initialize NimBLE stack
5. Register HCI transport callbacks
6. Start event processing thread

---

## Event Loop Pattern

The ATBM implementation uses a dedicated thread to read events:

```c
void *event_thread(void *arg) {
    while (!quit) {
        sem_wait(&event_sem);  // Wait for SIGIO

        do {
            struct status_async event;
            ssize_t len = read(atbm_fd, &event, sizeof(event));

            if (len == sizeof(event)) {
                process_event(&event);
            }
        } while (!event.list_empty);  // Keep reading if more events pending
    }
}
```

---

## TX Data Path

### Sending HCI Commands/Data

```c
int send_hci_packet(uint8_t type, const uint8_t *data, size_t len) {
    uint8_t buffer[2048];
    uint16_t *plen = (uint16_t *)buffer;

    *plen = len + 1;  // Total length including type byte
    buffer[2] = type;  // BLE_HCI_HIF_CMD or BLE_HCI_HIF_ACL
    memcpy(&buffer[3], data, len);

    return ioctl(atbm_fd, ATBM_BLE_HIF_TXDATA, (unsigned long)buffer);
}
```

### Thread Safety

The ioctl operations should be protected with a semaphore to prevent concurrent access:

```c
sem_wait(&ioctl_sem);
ioctl(atbm_fd, ATBM_BLE_HIF_TXDATA, buffer);
sem_post(&ioctl_sem);
```

---

## NimBLE Integration

The ATBM transport layer integrates with Apache NimBLE v4.2:

### Key Functions

**TX from Host Stack:**
- `ble_hci_trans_hs_cmd_tx()` - Send HCI command
- `ble_hci_trans_hs_acl_tx()` - Send ACL data

**RX to Host Stack:**
- `ble_hci_trans_hs_rx()` - Deliver received HCI data to stack

**Buffer Management:**
- `ble_hci_trans_buf_alloc()` - Allocate HCI buffer
- `ble_hci_trans_buf_free()` - Free HCI buffer
- `ble_hci_trans_acl_buf_alloc()` - Allocate ACL mbuf

---

## Error Codes

Standard Linux errno values are used:
- `0` - Success
- `-ENOMEM` - Out of memory
- `-EINVAL` - Invalid argument
- `-EIO` - I/O error
- `-ENOTTY` - Inappropriate ioctl for device

---

## Constants

```c
#define ATBM_IOCTL                  (121)
#define MAX_SYNC_EVENT_BUFFER_LEN   512
#define HCI_ACL_SHARE_SIZE          1538

// HCI packet types
#define BLE_HCI_HIF_NONE            0x00
#define BLE_HCI_HIF_CMD             0x01
#define BLE_HCI_HIF_ACL             0x02
#define BLE_HCI_HIF_SCO             0x03
#define BLE_HCI_HIF_EVT             0x04
#define BLE_HCI_HIF_ISO             0x05

// Message IDs (in WSM header)
#define HI_MSG_ID_BLE_BASE          0xC00
#define HI_MSG_ID_BLE_EVENT         (HI_MSG_ID_BLE_BASE + 0x01)
#define HI_MSG_ID_BLE_ACK           (HI_MSG_ID_BLE_BASE + 0x02)

// Alternative IDs for combined WiFi+BLE mode
#define HI_MSG_ID_BLE_BIT           BIT(8)
#define HI_MSG_ID_BLE_BASE_COMB     (0x800 + HI_MSG_ID_BLE_BIT)
#define HI_MSG_ID_BLE_EVENT_COMB    (HI_MSG_ID_BLE_BASE_COMB + 0x03)
#define HI_MSG_ID_BLE_ACK_COMB      (HI_MSG_ID_BLE_BASE_COMB + 0x04)
```

---

## Example Usage

See `ble_hci_hif.c` for complete implementation example:
- `/Users/yinzara/github/atbm-wifi/ble_host/nimble_v42/nimble/transport/ioctl/ble_hci_hif.c`

---

## References

- **Transport Implementation:** `ble_host/nimble_v42/nimble/transport/ioctl/ble_hci_hif.c`
- **ioctl Definitions:** `ble_host/os/linux/atbm_os_api.c` (lines 275-277, 557-562)
- **Header Structures:** `ble_host/nimble_v42/nimble/transport/ioctl/ble_hci_ioctl.h`
- **NimBLE HCI API:** `ble_host/nimble_v42/nimble/host/include/host/ble_hci_trans.h`

---

## Notes

1. The ATBM device requires `ATBM_BLE_COEXIST_START` to be called before any BLE operations.
2. All HCI packets are wrapped in a length prefix (2 bytes) when transmitted.
3. Event notification uses Linux async I/O (SIGIO signal).
4. Events may be queued; check `list_empty` field to drain the queue.
5. The WSM header format is specific to ATBM and wraps standard HCI packets.
6. Thread synchronization is required for both TX and RX paths.
