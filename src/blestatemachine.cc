/*
 *
 *  libblepp - Implementation of the Generic ATTribute Protocol
 *
 *  Copyright (C) 2013, 2014 Edward Rosten
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

#include "libblepp/bledevice.h"
#include "libblepp/logging.h"
#include "libblepp/att_pdu.h"
#include "libblepp/pretty_printers.h"
#include "libblepp/blestatemachine.h"

#include <algorithm>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
using namespace std;



template<class C> const C& haxx(const C& X)
{
	return X;
}

static int haxx(uint8_t X)
{
	return X;
}
#define LOGVAR(X) LOG(Info,  #X << " = " << haxx(X))


#define log_fd(X) log_fd_(X, __LINE__, __FILE__)

int log_fd_(int fd, int line, const char* file)
{
	if(fd < 0)
	{
		LOG(Error, "Error on line: " << line << " (" << file << "): " << strerror(errno));
	}
	else
		LOG(Info, "Socket success: " << line << " (" << file << ")");

	return fd;
}

const char* BLEGATTStateMachine::get_disconnect_string(Disconnect d)
{
	switch(d)
	{
		case ConnectionFailed: return "Connection failed.";
		case UnexpectedError: return "Unexpected Error.";
		case UnexpectedResponse: return "Unexpected Response.";
		case WriteError: return "Write Error.";
		case ReadError: return "Read Error.";
		case ConnectionClosed: return "Connection Closed.";
		default: return "Unknown reason.";
	}
}



const ServiceInfo* lookup_service_by_UUID(const UUID& uuid)
{
	static vector<ServiceInfo> vec;

	if(vec.size() == 0)
	{
		auto add = [](const char* n, const char* i, uint16_t u)
		{
			ServiceInfo s;
			s.name=n;
			s.id = i;
			s.uuid = UUID(u);
			vec.push_back(s);
		};

		add( "Alert Notification Service",    "org.bluetooth.service.alert_notification",        0x1811);
		add( "Battery Service",               "org.bluetooth.service.battery_service",           0x180F);
		add( "Blood Pressure",                "org.bluetooth.service.blood_pressure",            0x1810);
		add( "Body Composition",              "org.bluetooth.service.body_composition",          0x181B);
		add( "Bond Management",               "org.bluetooth.service.bond_management",           0x181E);
		add( "Current Time Service",          "org.bluetooth.service.current_time",              0x1805);
		add( "Cycling Power",                 "org.bluetooth.service.cycling_power",             0x1818);
		add( "Cycling Speed and Cadence",     "org.bluetooth.service.cycling_speed_and_cadence", 0x1816);
		add( "Device Information",            "org.bluetooth.service.device_information",        0x180A);
		add( "Generic Access",                "org.bluetooth.service.generic_access",            0x1800);
		add( "Generic Attribute",             "org.bluetooth.service.generic_attribute",         0x1801);
		add( "Glucose",                       "org.bluetooth.service.glucose",                   0x1808);
		add( "Health Thermometer",            "org.bluetooth.service.health_thermometer",        0x1809);
		add( "Heart Rate",                    "org.bluetooth.service.heart_rate",                0x180D);
		add( "Human Interface Device",        "org.bluetooth.service.human_interface_device",    0x1812);
		add( "Immediate Alert",               "org.bluetooth.service.immediate_alert",           0x1802);
		add( "Link Loss",                     "org.bluetooth.service.link_loss",                 0x1803);
		add( "Location and Navigation",       "org.bluetooth.service.location_and_navigation",   0x1819);
		add( "Next DST Change Service",       "org.bluetooth.service.next_dst_change",           0x1807);
		add( "Phone Alert Status Service",    "org.bluetooth.service.phone_alert_status",        0x180E);
		add( "Reference Time Update Service", "org.bluetooth.service.reference_time_update",     0x1806);
		add( "Running Speed and Cadence",     "org.bluetooth.service.running_speed_and_cadence", 0x1814);
		add( "Scan Parameters",               "org.bluetooth.service.scan_parameters",           0x1813);
		add( "Tx Power",                      "org.bluetooth.service.tx_power",                  0x1804);
		add( "User Data",                     "org.bluetooth.service.user_data",                 0x181C);
		add( "Weight Scale",                  "org.bluetooth.service.weight_scale",              0x181D);
	}

	auto f = find_if(vec.begin(), vec.end(), [&](const ServiceInfo& s)
	{
		return s.uuid == uuid;
	});

	if(f == vec.end())
		return 0;
	else
		return &*f;
}	


void BLEGATTStateMachine::buggerall()
{
}

void BLEGATTStateMachine::buggerall2(Disconnect)
{
}


void BLEGATTStateMachine::close()
{
	reset();

	state = Disconnected;
	next_handle_to_read=-1;
	last_request=-1;

	if(sock != -1)
		log_fd(::close(sock));
	sock = -1;
}


int log_l2cap_options(int sock)
{
	//Read and log the socket setup.

	l2cap_options options;
	unsigned int len = sizeof(options);
	memset(&options, 0, len);

	//Get the options with a minor bit of cargo culting.
	//SOL_L2CAP seems to mean that is should operate at the L2CAP level of the stack
	//L2CAP_OPTIONS who knows?
	if(log_fd(getsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, &options, &len)) == -1)
		return -1;

	LOGVAR(options.omtu);
	LOGVAR(options.imtu);
	LOGVAR(options.flush_to);
	LOGVAR(options.mode);
	LOGVAR(options.fcs);
	LOGVAR(options.max_tx);
	LOGVAR(options.txwin_size);
	
	return 0;
}

BLEGATTStateMachine::~BLEGATTStateMachine()
{
	ENTER();
	close();
}

BLEGATTStateMachine::BLEGATTStateMachine()
:dev(sock)
{
	ENTER();
	close();
	buf.resize(128);
}

void BLEGATTStateMachine::connect_blocking(const string& address)
{
	connect(address, true);
}


void BLEGATTStateMachine::connect_nonblocking(const string& address)
{
	connect(address, false);
}

void BLEGATTStateMachine::connect(const string& address, bool blocking)
{
	ENTER();

	//The constructor sets up the socket. Unless something is badly broken,
	//then we should succeed. Therefore errors are an exception.

	//Allocate socket and create endpoint.
	//Make socket nonblocking so connect() doesn't hang.

	if(blocking)
		sock = log_fd(::socket(PF_BLUETOOTH, SOCK_SEQPACKET                 , BTPROTO_L2CAP));
	else
		sock = log_fd(::socket(PF_BLUETOOTH, SOCK_SEQPACKET | SOCK_NONBLOCK , BTPROTO_L2CAP));

	if(sock == -1)
		throw SocketAllocationFailed(strerror(errno));

	////////////////////////////////////////
	//Bind the socket
	//I believe that l2 is for an l2cap socket. These are kind of like
	//UDP in that they have port numbers and are packet oriented.
	//However they are also ordered and reliable.
	//So, a sockaddr_l2 has the family (obviously)
	//a PSM (wtf?) 
	//  Protocol Service Multiplexer (WTF?)
	//an address (of course)
	//a CID (wtf) 
	//  Channel ID (i.e. port number?)
	//and an address type (wtf)
	//Holy cargo cult, Batman!


	sockaddr_l2 sba;
	memset(&sba, 0, sizeof(sba));
	sba.l2_bdaddr={{0,0,0,0,0,0}};
	sba.l2_family=AF_BLUETOOTH;
	sba.l2_cid = htobs(LE_ATT_CID);
	sba.l2_bdaddr_type=BDADDR_LE_PUBLIC;
	log_fd(bind(sock, (sockaddr*)&sba , sizeof(sba)));

	memset(&addr, 0, sizeof(addr));
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_psm = 0;
	addr.l2_cid = htobs(LE_ATT_CID);


	//Address type: Low Energy public
	addr.l2_bdaddr_type=BDADDR_LE_PUBLIC;
	

	if(log_l2cap_options(sock) == -1)
	{
		reset();
		throw SocketGetSockOptFailed(strerror(errno));
	}
	//Construct an address from the address string
	
	
	


	//Can also use bacpy to copy addresses about
	int rr = str2ba(address.c_str(), &addr.l2_bdaddr);
	LOG(Debug, "address = " << address);
	LOG(Debug, "str2ba = " << rr);
	int ret = log_fd(::connect(sock, (sockaddr*)&addr, sizeof(addr)));
	


	if(ret == 0)
	{
		state = Idle;

		if(log_l2cap_options(sock) == -1)
		{
			reset();
			throw SocketGetSockOptFailed(strerror(errno));
		}

		cb_connected();
	}
	else if(errno == EINPROGRESS)
	{
		//This "error" means the connection is happening and
		//we should come back later after select() returns.
		state = Connecting;
	}
	else if(errno == ENETUNREACH || errno == EHOSTUNREACH)
	{
		close();
		cb_disconnected(ConnectionFailed);
	}
	else
	{
		reset();
		throw SocketConnectFailed(strerror(errno));
	}
}



int BLEGATTStateMachine::socket()
{
	return sock;
}

void BLEGATTStateMachine::reset()
{
	state = Idle;
	next_handle_to_read=-1;
	last_request=-1;
}




void BLEGATTStateMachine::state_machine_write()
{
	try
	{
		if(state == ReadingPrimaryService)
		{
			last_request = ATT_OP_READ_BY_GROUP_REQ;	
			dev.send_read_group_by_type(UUID(GATT_UUID_PRIMARY), next_handle_to_read, 0xffff);	
		}
		else if(state == FindAllCharacteristics)
		{
			last_request = ATT_OP_READ_BY_TYPE_REQ;	
			dev.send_read_by_type(UUID(GATT_CHARACTERISTIC), next_handle_to_read, 0xffff);	
		}
		else if(state == GetClientCharaceristicConfiguration)
		{
			last_request = ATT_OP_READ_BY_TYPE_REQ;	
			dev.send_read_by_type(UUID(GATT_CLIENT_CHARACTERISTIC_CONFIGURATION), next_handle_to_read, 0xffff);	
		}
		else if(state == AwaitingWriteResponse)
		{
			last_request = ATT_OP_WRITE_REQ;
			//data already sent
		}
	}
	catch(BLEDevice::WriteError)
	{
		fail(WriteError);
	}

}



////////////////////////////////////////////////////////////////////////////////
//
// Commands to move machine into other states explicitly
//

void BLEGATTStateMachine::read_primary_services()
{
	if(state != Idle)
		throw logic_error("Error trying to issue command mid state");
	state = ReadingPrimaryService;
	next_handle_to_read=1;
	state_machine_write();
}

void BLEGATTStateMachine::find_all_characteristics()
{
	if(state != Idle)
		throw logic_error("Error trying to issue command mid state");
	state = FindAllCharacteristics;
	next_handle_to_read=1;
	state_machine_write();
}

void BLEGATTStateMachine::get_client_characteristic_configuration()
{
	if(state != Idle)
		throw logic_error("Error trying to issue command mid state");
	state = GetClientCharaceristicConfiguration;
	next_handle_to_read=1;
	state_machine_write();
}

void BLEGATTStateMachine::set_notify_and_indicate(Characteristic& c, bool notify, bool indicate)
{
	LOG(Trace, "BLEGATTStateMachine::enable_indications(Characteristic&)");

	if(state != Idle)
		throw logic_error("Error trying to issue command mid state");
	
	if(!c.indicate && indicate)
		throw logic_error("Error: this is not indicateable");
	if(!c.notify && notify)
		throw logic_error("Error: this is not notifiable");

	//FIXME: check for CCC
	c.ccc_last_known_value = notify | (indicate << 1);


	try{
		dev.send_write_command(c.client_characteric_configuration_handle, c.ccc_last_known_value);
	}
	catch(BLEDevice::WriteError)
	{
		fail(WriteError);
	}
}


bool BLEGATTStateMachine::wait_on_write()
{
	if(state == Connecting)
		return true;
	else
		return false;
}

void BLEGATTStateMachine::fail(Disconnect d)
{
	close();
	cb_disconnected(d);
}

void BLEGATTStateMachine::unexpected_error(const PDUErrorResponse& r)
{
	PDUErrorResponse err(r);
	string msg = string("Received unexpected error:") + att_ecode2str(err.error_code());
	LOG(Error, msg);
	fail(UnexpectedError);
}
////////////////////////////////////////////////////////////////////////////////
//
// The state machine itself!
void BLEGATTStateMachine::write_and_process_next()
{
	ENTER();
	try
	{
		LOG(Debug, "State is: " << state);
		if(state == Connecting)
		{
			int errval=-7;
			socklen_t len;
			len = sizeof(errval);
			//Check the status of the socket
			log_fd(getsockopt(sock, SOL_SOCKET, SO_ERROR, &errval, &len));

			LOG(Info, "errval = " << strerror(errval));

			if(errval == 0)
			{
				//Connected, so go to the idle state
				reset();
				cb_connected();
			}
			else
			{
				close();
				cb_disconnected(ConnectionFailed);
			}

		}
		else
		{
			LOG(Error, "Not implemented!");
		}
	}
	catch(BLEDevice::WriteError)
	{
		fail(WriteError);
	}
	catch(BLEDevice::ReadError)
	{
		fail(ReadError);
	}
}

void BLEGATTStateMachine::read_and_process_next()
{
	ENTER();
	//This is always an error
	if(state == Connecting)
		throw logic_error("Trying to read socket while connecting");


	if(state == Disconnected)
	{	
		//This is just possible. Imagine select() returning both read and write.
		//The write is processed first and fails, causing a disconnect.
		//The program then issues a call to read without checking for errors.
		//The result is harmless and unlikely, so log a warning.
		LOG(Warning, "Trying to read_and_process_next while disconnected");
		return;
	}

	try
	{
		PDUResponse r = dev.receive(buf);

		if(r.type() == ATT_OP_HANDLE_NOTIFY || r.type() == ATT_OP_HANDLE_IND)
		{
			PDUNotificationOrIndication n(r);
			//Find the correct characteristic
			for(auto& s:primary_services)
				if(n.handle() > s.start_handle && n.handle() <= s.end_handle)
					for(auto& c:s.characteristics)
						if(n.handle() == c.value_handle)
						{
							if(c.cb_notify_or_indicate)
								c.cb_notify_or_indicate(n);
							else if(cb_notify_or_indicate)
								cb_notify_or_indicate(c, n);
							else
								LOG(Warning, "Notify arrived, but no callback set\n");
						}

			//Respond to indications after the callback has run
			if(!n.notification())
				dev.send_handle_value_confirmation();
		}
		else if(r.type() == ATT_OP_ERROR && PDUErrorResponse(r).request_opcode() != last_request)
		{
			PDUErrorResponse err(r);

			std::string msg = string("Unexpected opcode in error. Expected ") + att_op2str(last_request) + " got "  + att_op2str(err.request_opcode());
			LOG(Error, msg);
			fail(UnexpectedError);
		}
		else if(r.type() != ATT_OP_ERROR && r.type() != last_request + 1)
		{
			string msg = string("Unexpected response. Expected ") + att_op2str(last_request+1) + " got "  + att_op2str(r.type());
			LOG(Error, msg);
			fail(UnexpectedResponse);
		}
		else
		{
			if(state == ReadingPrimaryService)
			{
				if(r.type() == ATT_OP_ERROR)
				{
					if(PDUErrorResponse(r).error_code() == ATT_ECODE_ATTR_NOT_FOUND)
					{
						//Maybe ? Indicates that the last one has been read.
						cb_services_read();
						reset();
					}
					else
						unexpected_error(r);
				}
				else
				{
					GATTReadServiceGroup g(r);

					for(int i=0; i < g.num_elements(); i++)
					{
						struct PrimaryService service;
						service.start_handle = g.start_handle(i);
						service.end_handle   = g.end_handle(i);
						service.uuid         = UUID::from(g.uuid(i));
						primary_services.push_back(service);
					}


					if(primary_services.back().end_handle == 0xffff)
					{
						reset();
						cb_services_read();
					}
					else
					{
						next_handle_to_read = primary_services.back().end_handle+1;
						state_machine_write();
					}
				}
			}
			else if(state == FindAllCharacteristics)
			{
				if(r.type() == ATT_OP_ERROR)
				{
					if(PDUErrorResponse(r).error_code() == ATT_ECODE_ATTR_NOT_FOUND)
					{
						//Maybe ? Indicates that the last one has been read.
						reset();
						cb_find_characteristics();
					}
					else
						unexpected_error(r);
				}
				else
				{
					GATTReadCharacteristic rc(r);

					for(int i=0; i < rc.num_elements(); i++)
					{
						uint16_t handle = rc.handle(i);
						GATTReadCharacteristic::Characteristic ch = rc.characteristic(i);

						LOG(Debug, "Found characteristic handle: " << to_hex(handle));

						//Search for the correct service.
						for(unsigned int s=0; s < primary_services.size(); s++)
						{
							if(handle > primary_services[s].start_handle && handle <= primary_services[s].end_handle)
							{
								LOG(Debug, "  handle belongs to service " << s);
								Characteristic c(this);


								c.broadcast= ch.flags & GATT_CHARACTERISTIC_FLAGS_BROADCAST;
								c.read     = ch.flags & GATT_CHARACTERISTIC_FLAGS_READ;
								c.write_without_response= ch.flags & GATT_CHARACTERISTIC_FLAGS_WRITE_WITHOUT_RESPONSE;
								c.write    = ch.flags & GATT_CHARACTERISTIC_FLAGS_WRITE;
								c.notify   = ch.flags & GATT_CHARACTERISTIC_FLAGS_NOTIFY;
								c.indicate = ch.flags & GATT_CHARACTERISTIC_FLAGS_INDICATE;
								c.authenticated_write = ch.flags & GATT_CHARACTERISTIC_FLAGS_AUTHENTICATED_SIGNED_WRITES;
								c.extended = ch.flags & GATT_CHARACTERISTIC_FLAGS_EXTENDED_PROPERTIES;
								c.uuid     = UUID::from(ch.uuid);
								c.value_handle = ch.handle;
								c.client_characteric_configuration_handle = 0;
								c.first_handle = handle;

								//Initially mark the end as the start of the current service
								c.last_handle = primary_services[s].end_handle;

								//Terminate the previous characteristic
								if(!primary_services[s].characteristics.empty())
									primary_services[s].characteristics.back().last_handle = handle-1;

								primary_services[s].characteristics.push_back(c);



							}
						}

						next_handle_to_read = handle+1;
					}
					LOG(Debug,  "Reading " << to_hex((uint16_t)next_handle_to_read) << " next");
					state_machine_write();
				}
			}
			else if(state == GetClientCharaceristicConfiguration)
			{
				if(r.type() == ATT_OP_ERROR)
				{
					if(PDUErrorResponse(r).error_code() == ATT_ECODE_ATTR_NOT_FOUND)
					{
						//Maybe ? Indicates that the last one has been read.
						reset();
						cb_get_client_characteristic_configuration();
					}
					else
						unexpected_error(r);
				}
				else
				{
					GATTReadCCC rc(r);

					for(int i=0; i < rc.num_elements(); i++)
					{
						uint16_t handle = rc.handle(i);
						next_handle_to_read = handle + 1;
						LOG(Debug, "Handle: " << to_hex(rc.handle(i)) << "  ccc: " << to_hex(rc.ccc(i)));


						//Find the correct place
						for(auto& s:primary_services)
							if(handle > s.start_handle && handle <= s.end_handle)
								for(auto& c:s.characteristics)
									if(handle > c.first_handle && handle <= c.last_handle)
									{
										c.client_characteric_configuration_handle = rc.handle(i);
										c.ccc_last_known_value = rc.ccc(i);
									}

					}
					state_machine_write();
				}
			}
			else if(state == AwaitingWriteResponse)
			{

				if(r.type() == ATT_OP_ERROR)
					unexpected_error(r);
				else
				{
					reset();
					cb_write_response();
				}
			}
		}
	}
	catch(BLEDevice::WriteError)
	{
		fail(WriteError);
	}
	catch(BLEDevice::ReadError)
	{
		fail(ReadError);
	}
}
	

void BLEGATTStateMachine::send_write_request(uint16_t handle, const uint8_t* data, int length)
{
	if(state != Idle)
		throw logic_error("Error trying to issue command mid state");
	dev.send_write_request(handle, data, length);
	state = AwaitingWriteResponse;
	state_machine_write();
}

void Characteristic::write_request(const uint8_t*data, int length)
{
	s->send_write_request(value_handle, data, length);
}


void Characteristic::write_request(const uint8_t data)
{
	s->send_write_request(value_handle, &data, 1);
}

void Characteristic::set_notify_and_indicate(bool notify, bool indicate)
{
	LOG(Trace, "Characteristic::enable_indications()");
	s->set_notify_and_indicate(*this, notify, indicate);
}


void pretty_print_tree(const BLEGATTStateMachine& s)
{

	cout << "Primary services:\n";
	for(auto& service: s.primary_services)
	{
		cout << "Start: " << to_hex(service.start_handle);
		cout << " End:  " << to_hex(service.end_handle);
		cout << " UUID: " << to_str(service.uuid) << endl;
		const ServiceInfo* s = lookup_service_by_UUID(UUID::from(service.uuid));
		if(s)
			cout << "  " << s->id << ": " << s->name << endl;
		else
			cout << "  Unknown\n";


		for(auto& characteristic: service.characteristics)
		{
			cout  << "  Characteristic: " << to_str(characteristic.uuid) << endl;
			cout  << "   Start: " << to_hex(characteristic.first_handle) << "  End: " << to_hex(characteristic.last_handle) << endl;

			cout << "   Flags: ";
			if(characteristic.broadcast)
				cout << "Broadcast ";
			if(characteristic.read)
				cout << "Read ";
			if(characteristic.write_without_response)
				cout << "Write (without response) ";
			if(characteristic.write)
				cout << "Write ";
			if(characteristic.notify)
				cout << "Notify ";
			if(characteristic.indicate)
				cout << "Indicate ";
			if(characteristic.authenticated_write)
				cout << "Authenticated signed writes ";
			if(characteristic.extended)
				cout << "Extended properties ";
			cout  << endl;

			cout << "   Value at handle: " << characteristic.value_handle << endl;

			if(characteristic.client_characteric_configuration_handle != 0)
				cout << "   CCC: (" << to_hex(characteristic.client_characteric_configuration_handle) << ") " << to_hex(characteristic.ccc_last_known_value) << endl;

			cout << endl;

		}

		cout << endl;
	}
}


//Handy utility function to do the sort of thing you'd normally do.
void BLEGATTStateMachine::setup_standard_scan(std::function<void()>& cb)
{
	ENTER();

	cb_services_read = [this]()
	{
		this->find_all_characteristics();
	};


	cb_find_characteristics = [this]()
	{
		this->get_client_characteristic_configuration();
	};
	
	cb_get_client_characteristic_configuration = [&cb]()
	{	
		cb();
	};
	
	cb_connected = [this]()
	{
		read_primary_services();
	};
}
