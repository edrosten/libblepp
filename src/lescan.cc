#include "blepp/lescan.h"
#include "blepp/pretty_printers.h"
#include "blepp/gap.h"

#include <bluetooth/hci_lib.h>
#include <string>
#include <cstring>
#include <cerrno>
#include <iomanip>

using namespace std;

namespace BLEPP
{
	class Span
	{
		private:
			const uint8_t* begin_;
			const uint8_t* end_;

		public:
			Span(const std::vector<uint8_t>& d)
			:begin_(d.data()),end_(begin_ + d.size())
			{
			}

			Span(const Span&) = default;

			Span pop_front(size_t length)
			{
				if(length > size())
					throw std::out_of_range("");
					
				Span s = *this;
				s.end_ = begin_ + length;

				begin_ += length;	
				return s;
			}	
			const uint8_t* begin() const
			{
				return begin_;
			}
			const uint8_t* end() const
			{
				return end_;
			}
			
			const uint8_t& operator[](const size_t i) const
			{
				if(i >= size())
					throw std::out_of_range("");
				return begin_[i];
			}

			bool empty() const
			{
				return size()==0;
			}

			size_t size() const
			{
				return end_ - begin_;
			}

			const uint8_t* data() const
			{
				return begin_;
			}

			const uint8_t& pop_front()
			{
				if(begin_ == end_)
					throw std::out_of_range("");

				begin_++;
				return *(begin_-1);
			}
	};

	AdvertisingResponse::Flags::Flags(vector<uint8_t>&& s)
	:flag_data(s)
	{
		//Remove the type field
		flag_data.erase(flag_data.begin());
		if(!flag_data.empty())
		{
			//See 4.0/4.C.18.1
			LE_limited_discoverable =       flag_data[0] & (1<<0);
			LE_general_discoverable =       flag_data[0] & (1<<1);
			BR_EDR_unsupported =            flag_data[0] & (1<<2);
			simultaneous_LE_BR_controller = flag_data[0] & (1<<3);
			simultaneous_LE_BR_host =       flag_data[0] & (1<<4);
		}
	}

	string to_hex(const Span& s)
	{
		return to_hex(s.data(), s.size());
	}

	HCIScanner::Error::Error(const string& why)
	:std::runtime_error(why)
	{	
		LOG(LogLevels::Error, why);	
	}

	HCIScanner::IOError::IOError(const string& why, int errno_val)
	:Error(why + ": " +   strerror(errno_val))
	{
	}
		
	HCIScanner::HCIScanner(bool start_scan)
	:HCIScanner(start_scan, FilterDuplicates::Both, ScanType::Active)
	{
	}


	HCIScanner::HCIScanner(bool start_scan, FilterDuplicates filtering, ScanType st, string device)
	{
		if(filtering == FilterDuplicates::Hardware || filtering == FilterDuplicates::Both)
			hardware_filtering = true;
		else
			hardware_filtering = false;

		if(filtering == FilterDuplicates::Software || filtering == FilterDuplicates::Both)
			software_filtering = true;
		else
			software_filtering = false;

		scan_type=st;

		int	dev_id = 0;
		if (device == "") {
			//Get a route to any(?) BTLE adapter (?)
			dev_id = hci_get_route(NULL);
		}
		else {
			dev_id = hci_devid(device.c_str());
		}
		if (dev_id < 0) {
			throw HCIError("Error obtaining HCI device ID");
		}
		
		//Open the device
		//FIXME check errors
		hci_fd.set(hci_open_dev(dev_id));

		if(start_scan)
			start();
	}

	HCIScanner::HCIScanner()
	:HCIScanner(true)
	{

	}

	void HCIScanner::start()
	{
		ENTER();
		if(running)
		{
			LOG(Trace, "Scanner is already running");
			return;
		}

		//Cadged from the hcitool sources. No idea what
		//these mean
		uint16_t interval = htobs(0x0010);
		uint16_t window = htobs(0x0010);

		//Address for the adapter (I think). Use a public address.
		//other option is random. Works either way it seems.
		uint8_t own_type = LE_PUBLIC_ADDRESS;

		//Don't use a whitelist (?)
		uint8_t filter_policy = 0x00;

		
		//The 10,000 thing seems to be some sort of retry logic timeout
		//thing. Number of miliseconds, but there are multiple tries
		//where it gets reduced by 10ms each time. It's a bit odd.
		int err = hci_le_set_scan_parameters(hci_fd, static_cast<int>(scan_type), interval, window,
							own_type, filter_policy, 10000);
		if(err < 0)
		{
			if(errno != EIO)
				throw IOError("Setting scan parameters", errno);
			else
			{
				//If the BLE device is already set to scanning, then we get an IO error. So
				//try turning it off and trying again. This bad state would happen, if, to pick
				//like a *totally* hypothetical example, the program segged-out during scanning
				//and so never cleaned up properly.
				LOG(LogLevels::Warning, "Received I/O error while setting scan parameters.");
				LOG(LogLevels::Warning, "Switching off HCI scanner");
				err = hci_le_set_scan_enable(hci_fd, 0x00, 0x00, 10000);
				if(err < 0)
					throw IOError("Error disabling scan:", errno);


				err = hci_le_set_scan_parameters(hci_fd, static_cast<int>(scan_type), interval, window, own_type, filter_policy, 10000);
				if(err < 0)
					throw IOError("Error disabling scan:", errno);
				else
					LOG(LogLevels::Warning, "Setting scan parameters worked this time.");


			}
		}

		LOG(LogLevels::Info, "Starting scanner");
		scanned_devices.clear();

		//Removal of duplicates done on the adapter itself
		uint8_t filter_dup = hardware_filtering?0x01:0x00;
		
		
		//Set up the filters. 
		socklen_t olen = sizeof(old_filter);
		if (getsockopt(hci_fd, SOL_HCI, HCI_FILTER, &old_filter, &olen) < 0) 
			throw IOError("Getting HCI filter socket options", errno);

		
		//Magic incantations to get scan events
		struct hci_filter nf;
		hci_filter_clear(&nf);
		hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
		hci_filter_set_event(EVT_LE_META_EVENT, &nf);
		if (setsockopt(hci_fd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0)
			throw IOError("Setting HCI filter socket options", errno);


		//device disable/enable duplictes ????
		err = hci_le_set_scan_enable(hci_fd, 0x01, filter_dup, 10000);
		if(err < 0)
			throw IOError("Enabling scan", errno);

		running=true;
	}

	void HCIScanner::stop()
	{
		ENTER();
		if(!running)
		{
			return;
		}

		LOG(LogLevels::Info, "Cleaning up HCI scanner");
		int err = hci_le_set_scan_enable(hci_fd, 0x00, 0x00, 10000);

		if(err < 0)
			throw IOError("Error disabling scan:", errno);

		err = setsockopt(hci_fd, SOL_HCI, HCI_FILTER, &old_filter, sizeof(old_filter));

		if(err < 0)
			throw IOError("Error resetting HCI socket:", errno);

		running = false;
	}

	int HCIScanner::get_fd() const
	{
		return hci_fd;
	}

		
	HCIScanner::~HCIScanner()
	{
		try
		{
			stop();
		}
		catch(IOError)
		{
		}
	}
	
	HCIScanner::FilterEntry::FilterEntry(const AdvertisingResponse& a)
	:mac_address(a.address),type(static_cast<int>(a.type))
	{}
	
	bool HCIScanner::FilterEntry::operator<(const FilterEntry& f) const
	{
		//Simple lexi-sorting
		if(mac_address < f.mac_address)
			return true;
		else if(mac_address == f.mac_address)
			return type < f.type;
		else
			return false;
	}

	vector<uint8_t> HCIScanner::read_with_retry()
	{
		int len;
		std::vector<uint8_t> buf(HCI_MAX_EVENT_SIZE);


		while((len = read(hci_fd, buf.data(), buf.size())) < 0)
		{
			if(errno == EAGAIN)
				continue;
			else if(errno == EINTR)
				throw Interrupted("interrupted reading HCI packet");
			else
				throw IOError("reading HCI packet", errno);
		}

		buf.resize(len);
		return buf;
	}

	vector<AdvertisingResponse> HCIScanner::get_advertisements()
	{
		vector<AdvertisingResponse> adverts = parse_packet(read_with_retry());
		
		if(software_filtering)
		{
			vector<AdvertisingResponse> filtered;

			for(const auto& a: adverts)
			{
				auto r = scanned_devices.insert(FilterEntry(a));

				if(r.second)
					filtered.emplace_back(move(a));
				else
					LOG(Debug, "Entry " << a.address << " " << static_cast<int>(a.type) << " found already");
			}

			return filtered;
		}
		else
			return adverts;
	}

	/*
	   Hello comment-reader!

	   This is a class for dealing with device scanning. The scans are done
	   using the HCI (Host Controller Interface). Interstingly, the HCI is
	   well specified even down to the transport layer (RS232 syle, USB,
	   SDIO and some sort of SLIP based serial). The HCI sometimes unpacks
	   and aggregates PDUs (the data part of packets) before sending them
	   for some reason, so you need to look in the HCI part of the spec,
	   not the PDU part of the spec.

	   The HCI is also semi-autonomous, and can be instructed to do things like
	   sctive scanning (where it queries and reports additional information, 
	   typically the device name) and filtering of duplicated advertising 
	   events. This is great for low power draw because the host CPU doesn't need
	   to wake up to do menial or unimportant things.

	   Also, the HCI of course gives you everything! The kernel contains no 
	   permissioned control over filtering which means anything with permissions
	   to open the HCI can also get all data packets in both directions and
	   therefore sniff everything. As a result, scanning normally has to be 
	   activated by root only.

	   
	   The general HCI event format is (Bluetooth 4.0 Vol 2, Part E,  5.4.4)
	   <packet type> <event code> <length> <crap...>
	   
	   Note that the HCI unpacks some of the bitfields in the advertising
	   PDU and presents them as whole bytes.
	   
	   Packet type is not part of the HCI command. In USB the type is
	   determined by the endpoint address. In all the serial protocols the
	   packet type is 1 byte at the beginning where 
	   
	   Command = 0x01 Event = 0x04
	   
	   See, e.g. 4.0/4/A.2
	   
	   Linux seems to use these.

	   And the LE events are all <event code> = 0x3E (HCI_LE_META_EVT) as per
		4.0/2/E.7.7.65
		And the format of <crap...> is:
		<subevent code>
		<morecrap...>

	   And we're interested in advertising events, where 
		<subevent code> = 0x02 (as per 4.0/2/E.7.7.65.2)
	   
		And the format of <morecrap...> in this case is
		<num reports>
		<report[i]...>
	   
		Where <report> is
		<event type>
		<address type>
		<address>
		<length>
		<advertisment crap>
		<RSS>
	   
		<advertisement crap> is an advertising packet and that's specified
		all over the place. 4.0/3/C.8 for example. The advertising packets
		are of the format:
	   
		<length0> <type0> <data0> 
		<length1> <type1> <data1> ...
	   

	   Now, the meaning of type, and the valid data are not defined in the main
	   spec, but in "Supplement to the Bluetooth Core Specification" version 4, 
	   referred to here as S4. Except that  defines only the name of the types
	   not their numeric code for some reason. Those are on the web.
	   https://www.bluetooth.org/en-us/specification/assigned-numbers/generic-access-profile


	   Some datatypes are not valid in advertising packets and some are also not
	   valid in LE applications. Apart from a small selection, many of the types 
	   are rare to the point of nonexistence. I ignore them because I've got nothing
	   that generates tham and I wouldn't know what to do with them anyway. Feel free
	   to fix or ping me if you need them :)
		
	   The ones generally of interest are:
	   <<incomplete list of 16 bit UUIDs>>
	   <<complete list of 16 bit UUIDs>>
	   <<incomplete list of 128 bit UUIDs>>
	   <<complete list of 128 bit UUIDs>>
	   <<shortened local name>>
	   <<complete local name>>
	   <<flags>>
	   <<manufacturer specific data>>
	   <<service data>>
		 
	   Personal experience suggest UUIDs and flags are almost always present.
	   Names are often present. iBeacons use manufacturer specific data whereas
	   Eddystone beacons use service data. I've used neither. I've not personally
	   seen or heard of any of the other types, including 32 bit UUIDs.

	   There are also some moderately sensible restrictions. A device SHALL NOT
	   report bot incomplete and complete lists of the same things in the same
	   packet S4/1.1.1. Furthermore an ommitted UUID specification is equivalent 
	   to a incomplete list with no elements.



	   Returns 1 on success, 0  on failure - such as an inability
	   to handle the packet, not an error.
	   
	   Return code can probably be ignored because
	   it will call lambdas on specific packets anyway.
	   
	   TODO: replace some errors with throw.
	   such as the HCI device spewing crap.

	*/

	vector<AdvertisingResponse> parse_event_packet(Span packet);
	vector<AdvertisingResponse> parse_le_meta_event(Span packet);
	vector<AdvertisingResponse> parse_le_meta_event_advertisement(Span packet);

	vector<AdvertisingResponse> HCIScanner::parse_packet(const vector<uint8_t>& p)
	{
		Span  packet(p);
		LOG(Debug, to_hex(p));

		if(packet.size() < 1)
		{
			LOG(LogLevels::Error, "Empty packet received");
			return {};
		}

		uint8_t packet_id = packet.pop_front();


		if(packet_id == HCI_EVENT_PKT)
		{
			LOG(Debug, "Event packet received");
			return parse_event_packet(packet);
		}
		else
		{
			LOG(LogLevels::Error, "Unknown HCI packet received");
			throw HCIError("Unknown HCI packet received");
		}
	}

	vector<AdvertisingResponse> parse_event_packet(Span packet)
	{
		if(packet.size() < 2)
			throw HCIScanner::HCIError("Truncated event packet");
		
		uint8_t event_code = packet.pop_front();
		uint8_t length = packet.pop_front();

		
		if(packet.size() != length)
			throw HCIScanner::HCIError("Bad packet length");
		
		if(event_code == EVT_LE_META_EVENT)
		{
			LOG(Info, "event_code = 0x" << hex << (int)event_code << ": Meta event" << dec);
			LOGVAR(Info, length);

			return parse_le_meta_event(packet);
		}
		else
		{
			LOG(Info, "event_code = 0x" << hex << (int)event_code << dec);
			LOGVAR(Info, length);
			throw HCIScanner::HCIError("Unexpected HCI event packet");
		}
	}


	vector<AdvertisingResponse> parse_le_meta_event(Span packet)
	{
		uint8_t subevent_code = packet.pop_front();

		if(subevent_code == 0x02) // see big blob of comments above
		{
			LOG(Info, "subevent_code = 0x02: LE Advertising Report Event");
			return parse_le_meta_event_advertisement(packet);
		}
		else
		{
			LOGVAR(Info, subevent_code);
			return {};
		}
	}

	vector<AdvertisingResponse> parse_le_meta_event_advertisement(Span packet)
	{
		vector<AdvertisingResponse> ret;

		uint8_t num_reports = packet.pop_front();
		LOGVAR(Info, num_reports);

		for(int i=0; i < num_reports; i++)
		{
			LeAdvertisingEventType event_type = static_cast<LeAdvertisingEventType>(packet.pop_front());

			if(event_type == LeAdvertisingEventType::ADV_IND)
				LOG(Info, "event_type = 0x00 ADV_IND, Connectable undirected advertising");
			else if(event_type == LeAdvertisingEventType::ADV_DIRECT_IND)
				LOG(Info, "event_type = 0x01 ADV_DIRECT_IND, Connectable directed advertising");
			else if(event_type == LeAdvertisingEventType::ADV_SCAN_IND)
				LOG(Info, "event_type = 0x02 ADV_SCAN_IND, Scannable undirected advertising");
			else if(event_type == LeAdvertisingEventType::ADV_NONCONN_IND)
				LOG(Info, "event_type = 0x03 ADV_NONCONN_IND, Non connectable undirected advertising");
			else if(event_type == LeAdvertisingEventType::SCAN_RSP)
				LOG(Info, "event_type = 0x04 SCAN_RSP, Scan response");
			else
				LOG(Warning, "event_type = 0x" << hex << (int)event_type << dec << ", unknown");
			
			uint8_t address_type = packet.pop_front();

			if(address_type == 0)
				LOG(Info, "Address type = 0: Public device address");
			else if(address_type == 1)
				LOG(Info, "Address type = 0: Random device address");
			else
				LOG(Info, "Address type = 0x" << to_hex(address_type) << ": unknown");


			string address;
			for(int j=0; j < 6; j++)
			{
				ostringstream s;
				s << hex << setw(2) << setfill('0') << (int) packet.pop_front();
				if(j != 0)
					s << ":";

				address = s.str() + address;
			}


			LOGVAR(Info, address);

			uint8_t length = packet.pop_front();
			LOGVAR(Info, length);
			

			Span data = packet.pop_front(length);

			LOG(Debug, "Data = " << to_hex(data));

			int8_t rssi = packet.pop_front();

			if(rssi == 127)
				LOG(Info, "RSSI = 127: unavailable");
			else if(rssi <= 20)
				LOG(Info, "RSSI = " << (int) rssi << " dBm");
			else
				LOG(Info, "RSSI = " << to_hex((uint8_t)rssi) << " unknown");

			try{
				AdvertisingResponse rsp;
				rsp.address = address;
				rsp.type = event_type;
				rsp.rssi = rssi;

				while(data.size() > 0)
				{
					LOGVAR(Debug, data.size());
					LOG(Debug, "Packet = " << to_hex(data));
					//Format is length, type, crap
					int length = data.pop_front();
					
					LOGVAR(Debug, length);

					Span chunk = data.pop_front(length);
					uint8_t type = chunk[0];
					LOGVAR(Debug, type);

					if(type == GAP::flags)
					{
						rsp.flags = AdvertisingResponse::Flags({chunk.begin(), chunk.end()});

						LOG(Info, "Flags = " << to_hex(rsp.flags->flag_data));

						if(rsp.flags->LE_limited_discoverable)
							LOG(Info, "        LE limited discoverable");

						if(rsp.flags->LE_general_discoverable)
							LOG(Info, "        LE general discoverable");

						if(rsp.flags->BR_EDR_unsupported)
							LOG(Info, "        BR/EDR unsupported");

						if(rsp.flags->simultaneous_LE_BR_host)
							LOG(Info, "        simultaneous LE BR host");

						if(rsp.flags->simultaneous_LE_BR_controller)
							LOG(Info, "        simultaneous LE BR controller");
					}
					else if(type == GAP::incomplete_list_of_16_bit_UUIDs || type == GAP::complete_list_of_16_bit_UUIDs)
					{
						rsp.uuid_16_bit_complete = (type == GAP::complete_list_of_16_bit_UUIDs);
						chunk.pop_front(); //remove the type field

						while(!chunk.empty())
						{
							uint16_t u = chunk.pop_front() + chunk.pop_front()*256;
							rsp.UUIDs.push_back(UUID(u));
						}
					}
					else if(type == GAP::incomplete_list_of_128_bit_UUIDs || type == GAP::complete_list_of_128_bit_UUIDs)
					{
						rsp.uuid_128_bit_complete = (type == GAP::complete_list_of_128_bit_UUIDs);
						chunk.pop_front(); //remove the type field

						while(!chunk.empty())
							rsp.UUIDs.push_back(UUID::from(att_get_uuid128(chunk.pop_front(16).data())));
					}
					else if(type == GAP::shortened_local_name || type == GAP::complete_local_name)
					{
						chunk.pop_front();
						AdvertisingResponse::Name n;
						n.complete = type==GAP::complete_local_name;
						n.name = string(chunk.begin(), chunk.end());
						rsp.local_name = n;

						LOG(Info, "Name (" << (n.complete?"complete":"incomplete") << "): " << n.name);
					}
					else if(type == GAP::manufacturer_data)
					{
						chunk.pop_front();
						rsp.manufacturer_specific_data.push_back({chunk.begin(), chunk.end()});
						LOG(Info, "Manufacturer data: " << to_hex(chunk));
					}
					else
					{
						rsp.unparsed_data_with_types.push_back({chunk.begin(), chunk.end()});

						LOG(Info, "Unparsed chunk " << to_hex(chunk));
					}
				}

				if(rsp.UUIDs.size() > 0)
				{
					LOG(Info, "UUIDs (128 bit " << (rsp.uuid_128_bit_complete?"complete":"incomplete")
						  << ", 16 bit " << (rsp.uuid_16_bit_complete?"complete":"incomplete") << " ):");

					for(const auto& uuid: rsp.UUIDs)
						LOG(Info, "    " << to_str(uuid));
				}

				ret.push_back(rsp);


			}
			catch(out_of_range r)
			{
				LOG(LogLevels::Error, "Corrupted data sent by device " << address);
			}
		}

		return ret;
	}


}
