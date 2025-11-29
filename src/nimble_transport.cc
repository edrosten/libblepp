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

#include <blepp/blepp_config.h>

#ifdef BLEPP_NIMBLE_SUPPORT

#include <blepp/nimble_transport.h>
#include <blepp/gatt_services.h>
#include <blepp/logging.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <mutex>
#include <iomanip>

// NimBLE stack headers
extern "C" {
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"

// ATBM-specific NimBLE port functions
void nimble_port_atbmos_init(void(* host_task_fn)(void*));
void nimble_port_atbmos_free(void);
int hif_ioctl_init(void);
void ble_hs_sched_start(void);
}

// ATBM ioctl base number
#define ATBM_IOCTL                  (121)

// ATBM ioctl command definitions (from ble_host/os/linux/atbm_os_api.c)
#define ATBM_AT_CMD_DIRECT          _IOW(ATBM_IOCTL, 0, unsigned int)
#define ATBM_BLE_SMART              _IOW(ATBM_IOCTL, 1, unsigned int)

// ATBM BLE ioctl commands (from ble_host/nimble_v42/nimble/transport/ioctl/ble_hci_hif.c)
#define ATBM_BLE_COEXIST_START      _IOW(ATBM_IOCTL, 0, unsigned int)
#define ATBM_BLE_COEXIST_STOP       _IOW(ATBM_IOCTL, 1, unsigned int)
#define ATBM_BLE_SET_ADV_DATA       _IOW(ATBM_IOCTL, 2, unsigned int)
#define ATBM_BLE_ADV_RESP_MODE_START _IOW(ATBM_IOCTL, 3, unsigned int)
#define ATBM_BLE_SET_RESP_DATA      _IOW(ATBM_IOCTL, 4, unsigned int)
#define ATBM_BLE_HIF_TXDATA         _IOW(ATBM_IOCTL, 5, unsigned int)

// HCI packet types
#define BLE_HCI_HIF_NONE            0x00
#define BLE_HCI_HIF_CMD             0x01
#define BLE_HCI_HIF_ACL             0x02
#define BLE_HCI_HIF_SCO             0x03
#define BLE_HCI_HIF_EVT             0x04
#define BLE_HCI_HIF_ISO             0x05

// WSM message types
#define HI_MSG_ID_BLE_BASE          0xC00
#define HI_MSG_ID_BLE_EVENT         (HI_MSG_ID_BLE_BASE + 0x01)
#define HI_MSG_ID_BLE_ACK           (HI_MSG_ID_BLE_BASE + 0x02)

// Buffer sizes
#define MAX_SYNC_EVENT_BUFFER_LEN   512
#define HCI_ACL_SHARE_SIZE          1538

namespace BLEPP
{

// Global pointer to NimbleTransport instance for signal handler
static NimbleTransport* g_nimble_transport_instance = nullptr;
static std::mutex g_signal_mutex;

// Global variables for NimBLE stack synchronization
static uint8_t own_addr_type = 0;
static bool nimble_synced = false;
static bool gatts_started = false;
static std::mutex nimble_sync_mutex;
static sem_t nimble_sync_sem;

// NimBLE host sync callback - called when BLE stack is ready
static void nimble_sync_callback(void)
{
	int rc;

	LOG(Info, "NimBLE stack sync callback");

	// Ensure we have a valid address
	rc = ble_hs_util_ensure_addr(0);
	if (rc != 0) {
		LOG(Error, "Failed to ensure address: " << rc);
		return;
	}

	// Infer own address type
	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0) {
		LOG(Error, "Failed to infer address type: " << rc);
		return;
	}

	{
		std::lock_guard<std::mutex> lock(nimble_sync_mutex);
		nimble_synced = true;
	}

	// Services are already registered before the host task starts
	// No need to register them here

	// Signal that sync is complete
	sem_post(&nimble_sync_sem);

	LOG(Info, "NimBLE stack synchronized, address type: " << (int)own_addr_type);
}

// NimBLE GAP event callback - handles connection/disconnection events
static int nimble_gap_event_callback(struct ble_gap_event *event, void *arg)
{
	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
		LOG(Info, "GAP Connect event: status=" << event->connect.status);
		if (event->connect.status == 0) {
			LOG(Info, "Connected: handle=" << event->connect.conn_handle);
		} else {
			// Connection failed, restart advertising
			LOG(Info, "Connection failed, restarting advertising");
			std::lock_guard<std::mutex> lock(g_signal_mutex);
			if (g_nimble_transport_instance) {
				g_nimble_transport_instance->restart_advertising();
			}
		}
		break;

	case BLE_GAP_EVENT_DISCONNECT:
		LOG(Info, "GAP Disconnect event: reason=" << event->disconnect.reason);
		// Resume advertising after disconnect
		{
			std::lock_guard<std::mutex> lock(g_signal_mutex);
			if (g_nimble_transport_instance) {
				g_nimble_transport_instance->restart_advertising();
			}
		}
		break;

	case BLE_GAP_EVENT_ADV_COMPLETE:
		LOG(Info, "GAP Advertising complete");
		break;

	case BLE_GAP_EVENT_SUBSCRIBE:
		LOG(Info, "GAP Subscribe event: handle=" << event->subscribe.attr_handle);
		break;

	case BLE_GAP_EVENT_CONN_UPDATE:
		LOG(Info, "GAP Connection update: status=" << event->conn_update.status);
		break;

	case BLE_GAP_EVENT_MTU:
		LOG(Info, "GAP MTU update: conn_handle=" << event->mtu.conn_handle
		    << " channel_id=" << event->mtu.channel_id
		    << " mtu=" << event->mtu.value);
		break;

	case BLE_GAP_EVENT_NOTIFY_TX:
		LOG(Debug, "GAP Notify TX: conn_handle=" << event->notify_tx.conn_handle
		    << " attr_handle=" << event->notify_tx.attr_handle
		    << " status=" << event->notify_tx.status);
		break;

	case BLE_GAP_EVENT_NOTIFY_RX:
		LOG(Debug, "GAP Notify RX: conn_handle=" << event->notify_rx.conn_handle
		    << " attr_handle=" << event->notify_rx.attr_handle);
		break;

	default:
		LOG(Info, "GAP event: type=" << event->type << " (unknown)");
		break;
	}

	return 0;
}

// NimBLE host task function
static void nimble_host_task(void *param)
{
	// Set sync callback
	ble_hs_cfg.sync_cb = nimble_sync_callback;

	LOG(Info, "NimBLE host task starting");

	// Run NimBLE host stack event loop
	nimble_port_run();

	LOG(Info, "NimBLE host task stopped");
}

// Signal handler for SIGIO
static void nimble_signal_handler(int sig_num)
{
	if (sig_num == SIGIO) {
		std::lock_guard<std::mutex> lock(g_signal_mutex);
		if (g_nimble_transport_instance) {
			g_nimble_transport_instance->signal_event();
		}
	}
}

NimbleTransport::NimbleTransport(const char* device_path)
	: device_path_(device_path)
	, ioctl_fd_(-1)
	, advertising_(false)
	, next_conn_handle_(1)
	, host_task_started_(false)
	, adv_params_valid_(false)
	, running_(false)
{
	ENTER();

	// Open Nimble device
	ioctl_fd_ = open(device_path_.c_str(), O_RDWR);
	if (ioctl_fd_ < 0) {
		throw std::runtime_error(std::string("Failed to open ") + device_path_ + ": " + strerror(errno));
	}

	// Set up async I/O
	fcntl(ioctl_fd_, F_SETOWN, getpid());
	unsigned long flags = fcntl(ioctl_fd_, F_GETFL);
	fcntl(ioctl_fd_, F_SETFL, flags | FASYNC);

	// Set close-on-exec flag
	flags = fcntl(ioctl_fd_, F_GETFD, 0);
	fcntl(ioctl_fd_, F_SETFD, flags | FD_CLOEXEC);

	// Register signal handler
	{
		std::lock_guard<std::mutex> lock(g_signal_mutex);
		g_nimble_transport_instance = this;
		signal(SIGIO, nimble_signal_handler);
	}

	// Initialize semaphores
	sem_init(&event_sem_, 0, 0);
	sem_init(&ioctl_sem_, 0, 1);
	sem_init(&nimble_sync_sem, 0, 0);

	// Initialize NimBLE stack
	LOG(Info, "Initializing NimBLE stack");
	nimble_port_init();

	// Clear GATT database before registering services
	// This must be called AFTER nimble_port_init but BEFORE starting host task
	ble_gatts_reset();
	LOG(Info, "GATT database reset (before host task start)");

	// Initialize HCI ioctl interface
	if (hif_ioctl_init() < 0) {
		close(ioctl_fd_);
		throw std::runtime_error("Failed to initialize HCI ioctl interface");
	}

	// NOTE: We do NOT start the host task here!
	// Services must be registered before starting the host task.
	// The host task will be started in convert_and_register_services() after services are added.

	LOG(Info, "NimbleTransport initialized on " << device_path_ << " (host task will start after service registration)");
}

NimbleTransport::~NimbleTransport()
{
	ENTER();
	cleanup();
}

void NimbleTransport::cleanup()
{
	if (running_) {
		// Stop event thread
		running_ = false;
		sem_post(&event_sem_);  // Wake up event thread

		if (event_thread_.joinable()) {
			event_thread_.join();
		}

		// Stop advertising if active
		if (advertising_) {
			stop_advertising();
		}

		// Shutdown NimBLE stack
		LOG(Info, "Shutting down NimBLE stack");
		nimble_port_atbmos_free();

		// Cleanup signal handler
		{
			std::lock_guard<std::mutex> lock(g_signal_mutex);
			if (g_nimble_transport_instance == this) {
				g_nimble_transport_instance = nullptr;
				signal(SIGIO, SIG_DFL);
			}
		}

		// Close device
		if (ioctl_fd_ >= 0) {
			close(ioctl_fd_);
			ioctl_fd_ = -1;
		}

		// Destroy semaphores
		sem_destroy(&event_sem_);
		sem_destroy(&ioctl_sem_);
		sem_destroy(&nimble_sync_sem);

		LOG(Info, "NimbleTransport cleaned up");
	}
}

void NimbleTransport::signal_event()
{
	// Called from signal handler - post to semaphore to wake event thread
	sem_post(&event_sem_);
}

void NimbleTransport::event_loop_thread()
{
	LOG(Info, "Nimble event loop thread started");

	while (running_) {
		// Wait for SIGIO signal
		sem_wait(&event_sem_);

		if (!running_) {
			break;
		}

		// Read all pending events
		NimbleTransport::status_async event;
		do {
			ssize_t len = read(ioctl_fd_, &event, sizeof(event));

			if (len != sizeof(event)) {
				if (errno != EAGAIN && errno != EWOULDBLOCK) {
					LOG(Error, "Failed to read Nimble event: " << strerror(errno));
				}
				break;
			}

			// Process the event
			process_nimble_event(&event);

		} while (event.list_empty == 0);  // Continue if more events pending
	}

	LOG(Info, "Nimble event loop thread stopped");
}

void NimbleTransport::process_nimble_event(const struct status_async* event)
{
	// Extract WSM header
	const struct wsm_hdr* wsm = reinterpret_cast<const struct wsm_hdr*>(event->event_buffer);
	const uint8_t* hci_data = event->event_buffer + sizeof(struct wsm_hdr);

	LOG(Debug, "Nimble event: type=" << (int)event->type
	           << " driver_mode=" << (int)event->driver_mode
	           << " wsm_id=0x" << std::hex << wsm->id
	           << " wsm_len=" << std::dec << wsm->len);

	if (wsm->id == HI_MSG_ID_BLE_ACK || wsm->id == HI_MSG_ID_BLE_EVENT) {
		// Process HCI packet
		process_hci_event(hci_data, wsm->len);
	} else {
		LOG(Warning, "Unknown Nimble event ID: 0x" << std::hex << wsm->id);
	}
}

void NimbleTransport::process_hci_event(const uint8_t* data, size_t len)
{
	if (len < 2) {
		LOG(Error, "HCI event too short: " << len);
		return;
	}

	uint8_t hci_type = data[0];

	if (hci_type == BLE_HCI_HIF_EVT) {
		// HCI Event packet
		if (len < 3) {
			LOG(Error, "HCI event packet too short");
			return;
		}

		uint8_t event_code = data[1];
		uint8_t param_len = data[2];

		LOG(Debug, "HCI Event: code=0x" << std::hex << (int)event_code
		           << " param_len=" << std::dec << (int)param_len);

		// Parse specific HCI events
		switch (event_code) {
		case 0x03:  // HCI_Connection_Complete
			if (param_len >= 11) {
				uint8_t status = data[3];
				uint16_t conn_handle = (data[5] << 8) | data[4];

				if (status == 0 && on_connected) {
					// Extract peer address (6 bytes in reverse order for BLE)
					ConnectionParams conn_params;
					char addr_str[18];
					snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
					         data[11], data[10], data[9], data[8], data[7], data[6]);
					conn_params.peer_address = addr_str;
					conn_params.conn_handle = conn_handle;
					on_connected(conn_params);
					LOG(Info, "Connection complete: handle=" << conn_handle);
				} else {
					LOG(Error, "Connection failed: status=" << (int)status);
				}
			}
			break;

		case 0x05:  // HCI_Disconnection_Complete
			if (param_len >= 4) {
				uint8_t status = data[3];
				uint16_t conn_handle = (data[5] << 8) | data[4];
				uint8_t reason = data[6];

				if (status == 0 && on_disconnected) {
					on_disconnected(conn_handle);
					LOG(Info, "Disconnection complete: handle=" << conn_handle << " reason=" << (int)reason);
				}
			}
			break;

		case 0x0E:  // HCI_Command_Complete
			LOG(Debug, "Command complete event");
			break;

		case 0x0F:  // HCI_Command_Status
			if (param_len >= 3) {
				uint8_t status = data[3];
				if (status != 0) {
					LOG(Warning, "Command status error: " << (int)status);
				}
			}
			break;

		default:
			// For other events, pass to on_data_received if registered
			if (on_data_received) {
				// Try to extract connection handle if present
				uint16_t conn_handle = 0xFFFF;  // Invalid handle
				if (param_len >= 2) {
					conn_handle = (data[4] << 8) | data[3];
				}
				on_data_received(conn_handle, data + 3, param_len);
			}
			break;
		}

	} else if (hci_type == BLE_HCI_HIF_ACL) {
		// HCI ACL Data packet
		if (len < 5) {
			LOG(Error, "HCI ACL packet too short");
			return;
		}

		uint16_t handle_flags = (data[2] << 8) | data[1];
		uint16_t conn_handle = handle_flags & 0x0FFF;
		uint16_t data_len = (data[4] << 8) | data[3];

		LOG(Debug, "HCI ACL Data: conn_handle=" << conn_handle
		           << " data_len=" << data_len);

		if (on_data_received && len >= 5 + data_len) {
			on_data_received(conn_handle, data + 5, data_len);
		}
	}
}

int NimbleTransport::start_advertising(const AdvertisingParams& params)
{
	ENTER();

	if (advertising_) {
		LOG(Warning, "Already advertising");
		return 0;
	}

	// Store advertising parameters for potential restart after disconnect
	adv_params_ = params;
	adv_params_valid_ = true;

	// Set up advertising data using NimBLE GAP API
	struct ble_hs_adv_fields fields;
	memset(&fields, 0, sizeof(fields));

	// Add service UUIDs if provided
	static ble_uuid128_t service_uuids[8];  // Static storage for UUIDs
	if (!params.service_uuids.empty()) {
		size_t num_uuids = (params.service_uuids.size() < 8) ? params.service_uuids.size() : 8;
		LOG(Info, "Adding " << num_uuids << " service UUIDs to advertising data");
		for (size_t i = 0; i < num_uuids; ++i) {
			// Convert UUID to NimBLE format (little-endian)
			memcpy(service_uuids[i].value, params.service_uuids[i].value.u128.data, 16);
			service_uuids[i].u.type = BLE_UUID_TYPE_128;

			// Log UUID bytes for debugging
			LOG(Debug, "UUID[" << i << "] bytes: "
			    << std::hex << std::setfill('0')
			    << std::setw(2) << (int)service_uuids[i].value[0] << " "
			    << std::setw(2) << (int)service_uuids[i].value[1] << " "
			    << std::setw(2) << (int)service_uuids[i].value[2] << " "
			    << std::setw(2) << (int)service_uuids[i].value[3]
			    << std::dec);
		}
		fields.uuids128 = service_uuids;
		fields.num_uuids128 = num_uuids;
		fields.uuids128_is_complete = 1;
	}

	// Add service data if provided (for Improv WiFi v2.0 compliance)
	static uint8_t svc_data_buf[31];  // Static storage for service data
	if (params.service_data_uuid16 != 0 && !params.service_data.empty()) {
		LOG(Info, "Adding service data for UUID16 0x" << std::hex << params.service_data_uuid16
		    << std::dec << " (" << params.service_data.size() << " bytes)");

		// Build service data: [UUID16 little-endian][data bytes]
		svc_data_buf[0] = params.service_data_uuid16 & 0xFF;
		svc_data_buf[1] = (params.service_data_uuid16 >> 8) & 0xFF;
		size_t data_len = params.service_data.size();
		if (data_len > sizeof(svc_data_buf) - 2) {
			data_len = sizeof(svc_data_buf) - 2;
			LOG(Warning, "Service data truncated to " << data_len << " bytes");
		}
		memcpy(svc_data_buf + 2, params.service_data.data(), data_len);

		fields.svc_data_uuid16 = svc_data_buf;
		fields.svc_data_uuid16_len = data_len + 2;  // UUID16 (2 bytes) + data
	}

	int rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		LOG(Error, "Failed to set advertising fields: " << rc);
		return -1;
	}
	LOG(Info, "Advertising fields set successfully");

	// Set scan response data with device name
	struct ble_hs_adv_fields rsp_fields;
	memset(&rsp_fields, 0, sizeof(rsp_fields));

	if (!params.device_name.empty()) {
		rsp_fields.name = (uint8_t*)params.device_name.c_str();
		rsp_fields.name_len = params.device_name.length();
		rsp_fields.name_is_complete = 1;
	}

	rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
	if (rc != 0) {
		LOG(Error, "Failed to set scan response fields: " << rc);
		return -1;
	}
	LOG(Info, "Scan response fields set successfully");

	// Set advertising parameters
	struct ble_gap_adv_params advp;
	memset(&advp, 0, sizeof(advp));
	advp.conn_mode = BLE_GAP_CONN_MODE_UND;  // Undirected connectable
	advp.disc_mode = BLE_GAP_DISC_MODE_GEN;  // General discoverable
	// Convert milliseconds to BLE advertising interval units (0.625ms)
	advp.itvl_min = (params.min_interval_ms * 1000) / 625;  // ms to 0.625ms units
	advp.itvl_max = (params.max_interval_ms * 1000) / 625;

	LOG(Info, "Starting advertising with interval " << advp.itvl_min
	    << "-" << advp.itvl_max << " (0.625ms units), own_addr_type=" << (int)own_addr_type);

	// Start advertising using NimBLE GAP API with event callback
	rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &advp, nimble_gap_event_callback, NULL);
	if (rc != 0) {
		LOG(Error, "Failed to start Nimble advertising: " << rc);
		return -1;
	}

	advertising_ = true;
	LOG(Info, "Nimble advertising started: " << params.device_name);
	return 0;
}

void NimbleTransport::restart_advertising()
{
	ENTER();

	if (!adv_params_valid_) {
		LOG(Warning, "Cannot restart advertising: no previous advertising parameters");
		return;
	}

	LOG(Info, "Restarting advertising after disconnect");

	// Reset advertising flag so start_advertising doesn't bail early
	advertising_ = false;

	// Restart advertising with stored parameters
	int rc = start_advertising(adv_params_);
	if (rc != 0) {
		LOG(Error, "Failed to restart advertising: " << rc);
	}
}

int NimbleTransport::stop_advertising()
{
	ENTER();

	if (!advertising_) {
		return 0;
	}

	// Stop advertising using NimBLE GAP API
	int rc = ble_gap_adv_stop();
	if (rc != 0) {
		LOG(Error, "Failed to stop Nimble advertising: " << rc);
		return -1;
	}

	advertising_ = false;
	LOG(Info, "Nimble advertising stopped");
	return 0;
}

// NimBLE GATT callback bridge structures
struct NimbleGATTCallbackContext
{
	GATTAccessCallback callback;
	void* user_arg;
};

// Static storage for NimBLE GATT structures (must persist)
static std::vector<ble_uuid128_t> nimble_service_uuids;
static std::vector<ble_uuid128_t> nimble_char_uuids;
static std::vector<ble_gatt_chr_def> nimble_characteristics;
static std::vector<ble_gatt_svc_def> nimble_services;
static std::vector<NimbleGATTCallbackContext> nimble_callbacks;

// NimBLE GATT access callback - bridges to our GATTAccessCallback
static int nimble_gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	LOG(Info, ">>> NimBLE GATT ACCESS CALLBACK <<<");
	LOG(Info, "conn_handle=" << conn_handle << " attr_handle=" << attr_handle << " op=" << (int)ctxt->op);

	NimbleGATTCallbackContext* ctx = static_cast<NimbleGATTCallbackContext*>(arg);
	if (!ctx || !ctx->callback) {
		LOG(Error, "NimBLE GATT callback context is null");
		return BLE_ATT_ERR_UNLIKELY;
	}

	// Convert NimBLE operation to our ATTAccessOp
	ATTAccessOp op;
	switch (ctxt->op) {
	case BLE_GATT_ACCESS_OP_READ_CHR:
		op = ATTAccessOp::READ_CHR;
		break;
	case BLE_GATT_ACCESS_OP_WRITE_CHR:
		op = ATTAccessOp::WRITE_CHR;
		break;
	case BLE_GATT_ACCESS_OP_READ_DSC:
		op = ATTAccessOp::READ_DSC;
		break;
	case BLE_GATT_ACCESS_OP_WRITE_DSC:
		op = ATTAccessOp::WRITE_DSC;
		break;
	default:
		LOG(Warning, "Unknown NimBLE GATT operation: " << ctxt->op);
		return BLE_ATT_ERR_UNLIKELY;
	}

	// Prepare data buffer
	std::vector<uint8_t> data;
	uint16_t offset = 0;  // NimBLE doesn't use offset for characteristic access

	if (op == ATTAccessOp::WRITE_CHR || op == ATTAccessOp::WRITE_DSC) {
		// For writes, extract data from mbuf chain
		size_t len = OS_MBUF_PKTLEN(ctxt->om);
		LOG(Debug, "Write operation: len=" << len);
		data.resize(len);
		os_mbuf_copydata(ctxt->om, 0, data.size(), data.data());
	}

	// Call our callback
	LOG(Debug, "Calling application callback: op=" << (int)op << " data_len=" << data.size());
	int result = ctx->callback(conn_handle, op, offset, data);
	LOG(Debug, "Application callback returned: " << result);

	if (op == ATTAccessOp::READ_CHR || op == ATTAccessOp::READ_DSC) {
		// For reads, copy data to mbuf
		LOG(Debug, "Read operation: result=" << result << " data_len=" << data.size());
		if (result == 0 && !data.empty()) {
			int rc = os_mbuf_append(ctxt->om, data.data(), data.size());
			if (rc != 0) {
				LOG(Error, "Failed to append data to mbuf: " << rc);
				return BLE_ATT_ERR_INSUFFICIENT_RES;
			}
			LOG(Debug, "Successfully appended " << data.size() << " bytes to mbuf");
		}
	}

	return result;
}

int NimbleTransport::convert_and_register_services()
{
	ENTER();

	if (service_defs_.empty()) {
		LOG(Info, "No services to register with NimBLE");
		return 0;
	}

	LOG(Info, "Converting " << service_defs_.size() << " services to NimBLE format");

	// Clear previous registrations
	nimble_service_uuids.clear();
	nimble_char_uuids.clear();
	nimble_characteristics.clear();
	nimble_services.clear();
	nimble_callbacks.clear();

	// Reserve space to avoid reallocation (which would invalidate pointers)
	size_t total_chars = 0;
	for (const auto& svc : service_defs_) {
		total_chars += svc.characteristics.size();
	}

	nimble_service_uuids.reserve(service_defs_.size());
	nimble_char_uuids.reserve(total_chars);
	nimble_callbacks.reserve(total_chars);
	nimble_services.reserve(service_defs_.size() + 1);  // +1 for terminator

	// Convert each service
	for (const auto& svc_def : service_defs_) {
		// Convert service UUID
		ble_uuid128_t svc_uuid;
		memcpy(svc_uuid.value, svc_def.uuid.value.u128.data, 16);
		svc_uuid.u.type = BLE_UUID_TYPE_128;
		nimble_service_uuids.push_back(svc_uuid);

		LOG(Info, "Converting service with " << svc_def.characteristics.size() << " characteristics");

		// Start characteristics array for this service
		size_t char_start_idx = nimble_characteristics.size();

		// Convert each characteristic
		for (const auto& char_def : svc_def.characteristics) {
			LOG(Debug, "Converting characteristic with flags=0x" << std::hex << char_def.flags << std::dec
			    << " has_callback=" << (char_def.access_cb ? "yes" : "no"));
			// Convert characteristic UUID
			ble_uuid128_t char_uuid;
			memcpy(char_uuid.value, char_def.uuid.value.u128.data, 16);
			char_uuid.u.type = BLE_UUID_TYPE_128;
			nimble_char_uuids.push_back(char_uuid);

			// Create callback context if callback exists
			ble_gatt_access_fn* access_cb_ptr = nullptr;
			void* cb_arg = nullptr;

			if (char_def.access_cb) {
				NimbleGATTCallbackContext ctx;
				ctx.callback = char_def.access_cb;
				ctx.user_arg = char_def.arg;
				nimble_callbacks.push_back(ctx);

				access_cb_ptr = nimble_gatt_access_cb;
				cb_arg = &nimble_callbacks.back();
				LOG(Debug, "Callback registered for characteristic");
			} else {
				LOG(Warning, "No callback registered for characteristic with flags=0x" << std::hex << char_def.flags << std::dec);
			}

			// Convert flags
			uint16_t nimble_flags = 0;
			if (char_def.flags & GATT_CHR_F_READ) nimble_flags |= BLE_GATT_CHR_F_READ;
			if (char_def.flags & GATT_CHR_F_WRITE) nimble_flags |= BLE_GATT_CHR_F_WRITE;
			if (char_def.flags & GATT_CHR_F_WRITE_NO_RSP) nimble_flags |= BLE_GATT_CHR_F_WRITE_NO_RSP;
			if (char_def.flags & GATT_CHR_F_NOTIFY) nimble_flags |= BLE_GATT_CHR_F_NOTIFY;
			if (char_def.flags & GATT_CHR_F_INDICATE) nimble_flags |= BLE_GATT_CHR_F_INDICATE;

			// Create NimBLE characteristic definition
			ble_gatt_chr_def chr = {
				.uuid = &nimble_char_uuids.back().u,
				.access_cb = access_cb_ptr,
				.arg = cb_arg,
				.descriptors = nullptr,  // TODO: Support descriptors
				.flags = nimble_flags,
				.min_key_size = char_def.min_key_size,
				.val_handle = char_def.val_handle_ptr
			};

			nimble_characteristics.push_back(chr);
		}

		// Add terminator for characteristics array
		ble_gatt_chr_def chr_term = {0};
		nimble_characteristics.push_back(chr_term);

		// Create NimBLE service definition
		ble_gatt_svc_def svc = {
			.type = static_cast<uint8_t>((svc_def.type == GATTServiceType::PRIMARY) ?
			        BLE_GATT_SVC_TYPE_PRIMARY : BLE_GATT_SVC_TYPE_SECONDARY),
			.uuid = &nimble_service_uuids.back().u,
			.includes = nullptr,  // TODO: Support included services
			.characteristics = &nimble_characteristics[char_start_idx]
		};

		nimble_services.push_back(svc);
	}

	// Add terminator for services array
	ble_gatt_svc_def svc_term = {0};
	nimble_services.push_back(svc_term);

	// Register services with NimBLE GATTS
	LOG(Info, "Registering " << nimble_services.size() - 1 << " services ("
	    << nimble_characteristics.size() << " total characteristic entries) with NimBLE GATTS");

	int rc = ble_gatts_count_cfg(nimble_services.data());
	if (rc != 0) {
		LOG(Error, "Failed to count NimBLE GATT services: " << rc);
		return -1;
	}
	LOG(Debug, "ble_gatts_count_cfg returned: " << rc);

	rc = ble_gatts_add_svcs(nimble_services.data());
	if (rc != 0) {
		LOG(Error, "Failed to add NimBLE GATT services: " << rc);
		return -1;
	}
	LOG(Debug, "ble_gatts_add_svcs returned: " << rc);

	LOG(Info, "Successfully registered " << service_defs_.size() << " services with NimBLE");
	return 0;
}

int NimbleTransport::register_services(const std::vector<GATTServiceDef>& services)
{
	ENTER();

	// Store service definitions for later use
	service_defs_ = services;

	LOG(Info, "Storing " << services.size() << " GATT service definitions");

	// Register services with NimBLE IMMEDIATELY (before host task starts)
	int rc = convert_and_register_services();
	if (rc != 0) {
		LOG(Error, "Failed to register services: " << rc);
		return rc;
	}

	// Now that services are registered, start the NimBLE host task (if not already started)
	if (!host_task_started_) {
		LOG(Info, "Starting NimBLE host task after service registration");

		// Start NimBLE host task (this will trigger the sync callback)
		nimble_port_atbmos_init(nimble_host_task);

		// Start BLE host scheduler (this triggers the sync callback)
		LOG(Info, "Starting BLE host scheduler");
		ble_hs_sched_start();

		//Wait for NimBLE stack to synchronize (with timeout)
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 5;  // 5 second timeout

		LOG(Info, "Waiting for NimBLE stack to synchronize...");
		if (sem_timedwait(&nimble_sync_sem, &ts) < 0) {
			LOG(Error, "Timeout waiting for NimBLE stack synchronization");
			return -1;
		}

		LOG(Info, "NimBLE stack synchronized successfully");

		// Start event loop thread
		running_ = true;
		event_thread_ = std::thread(&NimbleTransport::event_loop_thread, this);

		host_task_started_ = true;
		LOG(Info, "NimBLE host task and event loop started");
	}

	return 0;
}

bool NimbleTransport::is_advertising() const
{
	return advertising_;
}

int NimbleTransport::accept_connection()
{
	// For Nimble, connections are handled asynchronously via events
	// The event loop thread will call on_connected when a connection occurs
	return 0;
}

int NimbleTransport::disconnect(uint16_t conn_handle)
{
	ENTER();

	std::lock_guard<std::mutex> lock(connections_mutex_);

	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		LOG(Warning, "Connection " << conn_handle << " not found");
		return -1;
	}

	// Send HCI Disconnect command
	uint8_t hci_disconnect[] = {
		0x06, 0x04,  // HCI_Disconnect opcode (0x0406)
		0x03,        // Parameter length
		(uint8_t)(conn_handle & 0xFF),
		(uint8_t)(conn_handle >> 8),
		0x13         // Reason: Remote User Terminated Connection
	};

	int ret = send_hci_command(hci_disconnect, sizeof(hci_disconnect));
	if (ret < 0) {
		LOG(Error, "Failed to send HCI disconnect command");
		return -1;
	}

	// Remove from connections map (actual disconnect event will come later)
	connections_.erase(it);

	LOG(Info, "Disconnecting connection " << conn_handle);
	return 0;
}

int NimbleTransport::send_pdu(uint16_t conn_handle, const uint8_t* data, size_t len)
{
	if (len > HCI_ACL_SHARE_SIZE) {
		LOG(Error, "PDU too large: " << len);
		return -1;
	}

	// Build HCI ACL packet with L2CAP header
	// Format:
	//  [0-1]: Length (total packet length for ioctl)
	//  [2]:   Packet type (BLE_HCI_HIF_ACL = 0x02)
	//  [3-4]: HCI ACL handle + flags
	//  [5-6]: HCI ACL data length (L2CAP length + 4 for L2CAP header)
	//  [7-8]: L2CAP length (ATT PDU length)
	//  [9-10]: L2CAP CID (0x0004 for ATT)
	//  [11+]: ATT PDU data

	uint8_t hci_packet[HCI_ACL_SHARE_SIZE + 20];
	size_t offset = 0;

	// [0-1]: Total length for ioctl = 1 (type) + 4 (HCI ACL hdr) + 4 (L2CAP hdr) + len (ATT data)
	uint16_t total_len = 1 + 4 + 4 + len;
	hci_packet[offset++] = total_len & 0xFF;
	hci_packet[offset++] = (total_len >> 8) & 0xFF;

	// [2]: Packet type
	hci_packet[offset++] = BLE_HCI_HIF_ACL;

	// [3-4]: HCI ACL handle + flags (PB=00, BC=00 for start of L2CAP PDU)
	hci_packet[offset++] = conn_handle & 0xFF;
	hci_packet[offset++] = (conn_handle >> 8) & 0x0F;

	// [5-6]: HCI ACL data length (L2CAP header + ATT data)
	uint16_t acl_len = 4 + len;  // L2CAP header (4) + ATT PDU
	hci_packet[offset++] = acl_len & 0xFF;
	hci_packet[offset++] = (acl_len >> 8) & 0xFF;

	// [7-8]: L2CAP length (just the ATT PDU)
	hci_packet[offset++] = len & 0xFF;
	hci_packet[offset++] = (len >> 8) & 0xFF;

	// [9-10]: L2CAP CID (0x0004 for ATT)
	hci_packet[offset++] = 0x04;
	hci_packet[offset++] = 0x00;

	// [11+]: ATT PDU data
	memcpy(&hci_packet[offset], data, len);
	offset += len;

	sem_wait(&ioctl_sem_);
	int ret = ioctl(ioctl_fd_, ATBM_BLE_HIF_TXDATA, (unsigned long)hci_packet);
	sem_post(&ioctl_sem_);

	if (ret < 0) {
		LOG(Error, "Failed to send HCI ACL data: " << strerror(errno));
		return -1;
	}

	LOG(Debug, "Sent " << len << " bytes ATT data on connection " << conn_handle
	           << " (total HCI packet: " << offset << " bytes)");
	return len;
}

int NimbleTransport::recv_pdu(uint16_t conn_handle, uint8_t* buf, size_t len)
{
	// For Nimble, data is received asynchronously via events
	// This function is not used in the async model
	return -1;
}

int NimbleTransport::send_hci_command(const uint8_t* cmd, size_t len)
{
	if (len > 258) {  // Max HCI command size
		LOG(Error, "HCI command too large: " << len);
		return -1;
	}

	// Build HCI command packet
	uint8_t hci_packet[260];
	uint16_t* plen = reinterpret_cast<uint16_t*>(hci_packet);

	*plen = len + 1;  // Type byte + command data
	hci_packet[2] = BLE_HCI_HIF_CMD;
	memcpy(&hci_packet[3], cmd, len);

	sem_wait(&ioctl_sem_);
	int ret = ioctl(ioctl_fd_, ATBM_BLE_HIF_TXDATA, (unsigned long)hci_packet);
	sem_post(&ioctl_sem_);

	if (ret < 0) {
		LOG(Error, "Failed to send HCI command: " << strerror(errno));
		return -1;
	}

	LOG(Debug, "Sent HCI command: " << len << " bytes");
	return 0;
}

int NimbleTransport::set_mtu(uint16_t conn_handle, uint16_t mtu)
{
	std::lock_guard<std::mutex> lock(connections_mutex_);

	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		return -1;
	}

	it->second.mtu = mtu;
	LOG(Info, "Set MTU for connection " << conn_handle << " to " << mtu);
	return 0;
}

uint16_t NimbleTransport::get_mtu(uint16_t conn_handle) const
{
	std::lock_guard<std::mutex> lock(connections_mutex_);

	auto it = connections_.find(conn_handle);
	if (it == connections_.end()) {
		return 23;  // Default ATT MTU
	}

	return it->second.mtu;
}

int NimbleTransport::process_events()
{
	// Events are processed asynchronously by the event thread
	// This function is for compatibility with the transport interface
	return 0;
}

} // namespace BLEPP

#endif // BLEPP_NIMBLE_SUPPORT
