#ifndef __INC_LIBATTGATT_BLEDEVICE_H
#define __INC_LIBATTGATT_BLEDEVICE_H

#include <cstdint>
#include <vector>
#include <string>
#include <libattgatt/att_pdu.h>

//Almost zero resource to represent the ATT protocol on a BLE
//device. This class does none of its own memory management, and will not generally allocate
//or do other nasty things. Oh no, it allocates a buffer! FIXME!
//
//Mostly what it can do is write ATT command packets (PDUs) and receive PDUs back.
struct BLEDevice
{
	bool constructing;
	int sock;
	static const int buflen=ATT_DEFAULT_MTU;
	std::vector<std::uint8_t> buf;

	void test_fd_(int fd, int line);
	void test_pdu(int len);
	BLEDevice(const std::string&);

	void send_read_by_type(const bt_uuid_t& uuid, std::uint16_t start = 0x0001, std::uint16_t end=0xffff);
	void send_find_information(std::uint16_t start = 0x0001, std::uint16_t end=0xffff);
	void send_read_group_by_type(const bt_uuid_t& uuid, std::uint16_t start = 0x0001, std::uint16_t end=0xffff);
	void send_write_request(std::uint16_t handle, const std::uint8_t* data, int length);
	void send_write_request(std::uint16_t handle, std::uint16_t data);
	void send_handle_value_confirmation();
	void send_write_command(std::uint16_t handle, const std::uint8_t* data, int length);
	void send_write_command(std::uint16_t handle, std::uint16_t data);
	PDUResponse receive(std::uint8_t* buf, int max);
	PDUResponse receive(std::vector<std::uint8_t>& v);
};

#endif
