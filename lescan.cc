#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <cerrno>
#include <array>
#include <iomanip>
#include <vector>

#include <stdexcept>

#include <libattgatt/logging.h>
#include <libattgatt/pretty_printers.h>

using namespace std;


class Error: public std::runtime_error 
{
	public:
	Error(const string& why, int errno_val)
	:std::runtime_error(why  + ": " +   strerror(errno_val))
	{
	}
};

template<class T>
class Span
{
	private:
		const T* begin;
		const T* end;

	public:
		Span(const std::vector<T>& d)
		:begin(d.data()),end(begin + d.size())
		{
		}

		Span(const Span&) = default;

		Span pop_front(size_t length)
		{
			if(length > size())
				throw std::out_of_range("");
				
			Span s = *this;
			s.end = begin + length;

			begin += length;	
			return s;
		}	

		const T& operator[](const size_t i) const
		{
			//FIXME check bounds
			if(i >= size())
				throw std::out_of_range("");
			return begin[i];
		}

		size_t size() const
		{
			return end - begin;
		}

		const T* data() const
		{
			return begin;
		}

		const T& pop_front()
		{
			if(begin == end)
				throw std::out_of_range("");

			begin++;
			return *(begin-1);
		}
};

namespace HCI
{
	enum LeAdvertisingEventType
	{	
		ADV_IND = 0x00, //Connectable undirected advertising 
 		                //Broadcast; any device can connect or ask for more information
		ADV_DIRECT_IND = 0x01, //Connectable Directed
		                       //Targeted; a single known device that can only connect
		ADV_SCAN_IND = 0x02, //Scannable Undirected
		                     //Purely informative broadcast; devices can ask for more information
		ADV_NONCONN_IND = 0x03, //Non-Connectable Undirected
		                        //Purely informative broadcast; no device can connect or even ask for more information
		SCAN_RSP = 0x04, //Result coming back after a scan request
	};

}


class HCIScanner
{
	public:
	
	class Error: public ::Error
	{
		using ::Error::Error;
	};

	class Interrupted: public Error
	{
		using Error::Error;
	};

	class IOError: public Error
	{
		using Error::Error;
	};

	HCIScanner()
	{
		//Get a route to any(?) BTLE adapter (?)
		//FIXME check errors
		int	dev_id = hci_get_route(NULL);
		
		//Open the device
		//FIXME check errors
		hci_fd = hci_open_dev(dev_id);

		//0 = passive scan
		//1 = active scan
		int scan_type  = 0x00;

		//Cadged from the hcitool sources. No idea what
		//these mean
		uint16_t interval = htobs(0x0010);
		uint16_t window = htobs(0x0010);

		//Removal of duplicates done on the adapter itself
		uint8_t filter_dup = 0x01;
		
		//Address for the adapter (I think). Use a public address.
		//other option is random. Works either way it seems.
		uint8_t own_type = LE_PUBLIC_ADDRESS;

		//Don't use a whitelist (?)
		uint8_t filter_policy = 0x00;

		
		//The 10,000 thing seems to be some sort of retry logic timeout
		//thing. Number of miliseconds, but there are multiple tries
		//where it gets reduced by 10ms each time. It's a bit odd.
		int err = hci_le_set_scan_parameters(hci_fd, scan_type, interval, window,
							own_type, filter_policy, 10000);
		
		if(err < 0)
			throw IOError("Setting scan parameters", errno);

		//device disable/enable duplictes ????
		err = hci_le_set_scan_enable(hci_fd, 0x01, filter_dup, 10000);
		if(err < 0)
			throw IOError("Enabling scan", errno);

		
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


	}

	int get_fd() const
	{
		return hci_fd;
	}

	
	~HCIScanner()
	{
		setsockopt(hci_fd, SOL_HCI, HCI_FILTER, &old_filter, sizeof(old_filter));
	}


	std::vector<uint8_t> read_with_retry()
	{
		int len;
		std::vector<uint8_t> buf(HCI_MAX_EVENT_SIZE);


		while((len = read(hci_fd, buf.data(), buf.size())) < 0)
		{
			if(errno == EAGAIN)
				continue;
			else if(errno == EINTR)
				throw Interrupted("reading HCI packet", EINTR);
			else
				throw IOError("reading HCI packet", errno);
		}

		buf.resize(len);
		return buf;
	}


	private:
		int hci_fd=-1;
		hci_filter old_filter;


		//The general HCI event format is (Bluetooth 4.0 Vol 2, Part E,  5.4.4
		// <packet type>
		// <event code>
		// <length>
		// <crap...>
		//
		//Note that the HCI unpacks some of the bitfields in the advertising 
		//PDU and presents them as whole bytes.
		
		//Packet type is not part of the HCI command. In USB the type is
		//determined by the endpoint address. In all the serial protocols
		//the packet type is 1 byte at the beginning where 
		//
		// Command = 0x01
		// Event = 0x04
		//
		// See, e.g. 4.0/4/A.2
		//
		//Linux seems to use these.

		//And the LE events are all <event code> = 0x3E (HCI_LE_META_EVT) as per
		// 4.0/2/E.7.7.65
		// And the format of <crap...> is:
		// <subevent code>
		// <morecrap...>

		//And we're interested in advertising events, where 
		// <subevent code> = 0x02 (as per 4.0/2/E.7.7.65.2)
		//
		// And the format of <morecrap...> in this case is
		// <num reports>
		// <report[i]...>
		//
		// Where <report> is
		// <event type>
		// <address type>
		// <address>
		// <length>
		// <advertisment crap>
		// <RSS>
		
		// <advertisement crap> is an advertising packet and that's specified
		// all over the place. 4.0/3/C.8 for example. The advertising packets
		// are of the format:
		//
		// <length0> <type0> <data0> 
		// <length1> <type1> <data1> ...
		//



		//Returns 1 on success, 0  on failure - such as an inability
		//to handle the packet, not an error.
		//
		//Return code can probably be ignored because
		//it will call lambdas on specific packets anyway.
		//
		//TODO: replace some errors with throw.
		//such as the HCI device spewing crap.
		bool parse_packet(const vector<uint8_t>& p)
		{
			Span<uint8_t> packet(p);
			LOG(Debug, to_hex(p));

			if(packet.size() < 1)
			{
				LOG(LogLevels::Error, "Empty packet received");
				return 0;
			}

			uint8_t packet_id = packet.pop_front();


			if(packet_id == HCI_EVENT_PKT)
			{
				LOG(Debug, "Event packet received");
				return parse_event_packet(packet);
			}
			else
			{
				return 0;
				LOG(Debug, "Unknown packet received");
			}
		}

		bool parse_event_packet(Span<uint8_t> packet)
		{
			if(packet.size() < 2)
			{
				LOG(LogLevels::Error, "Truncated event packet");
				return 0;
			}
			
			uint8_t event_code = packet.pop_front();
			uint8_t length = packet.pop_front();

			
			if(packet.size() != length)
			{
				LOG(LogLevels::Error, "Bad packet length");
				return 0;
			}
			
			if(event_code == EVT_LE_META_EVENT)
			{
				LOG(Info, "event_code = 0x" << hex << (int)event_code << ": Meta event" << dec);
				LOGVAR(Info, length);

				return parse_le_meta_event(packet);
			}
			else
			{
				LOG(Info, "event_code = 0x" << hex << (int)event_code << ": Meta event" << dec);
				LOGVAR(Info, length);
				return 0;
			}
		}


		bool parse_le_meta_event(Span<uint8_t> packet)
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
				return 0;
			}
		}
		
		bool parse_le_meta_event_advertisement(Span<uint8_t> packet)
		{
			uint8_t num_reports = packet.pop_front();
			LOGVAR(Info, num_reports);

			for(int i=0; i < num_reports; i++)
			{
				uint8_t event_type = packet.pop_front();

				if(event_type == HCI::ADV_IND)
					LOG(Info, "event_type = 0x00 ADV_IND, Connectable undirected advertising");
				else if(event_type == HCI::ADV_DIRECT_IND)
					LOG(Info, "event_type = 0x01 ADV_DIRECT_IND, Connectable directed advertising");
				else if(event_type == HCI::ADV_SCAN_IND)
					LOG(Info, "event_type = 0x02 ADV_SCAN_IND, Scannable undirected advertising");
				else if(event_type == HCI::ADV_NONCONN_IND)
					LOG(Info, "event_type = 0x03 ADV_NONCONN_IND, Non connectable undirected advertising");
				else if(event_type == HCI::SCAN_RSP)
					LOG(Info, "event_type = 0x04 SCAN_RSP, Scan response");
				else
					LOG(Info, "event_type = 0x" << hex << (int)event_type << dec << ", unknown");
				
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
				
				Span<uint8_t> data = packet.pop_front(length);

				int8_t rssi = data.pop_front();

				if(rssi == 127)
					LOG(Info, "RSSI = 127: unavailable");
				else if(rssi <= 20)
					LOG(Info, "RSSI = " << (int) rssi << " dBm");
				else
					LOG(Info, "RSSI = " << to_hex((uint8_t)rssi) << " unknown");
			}

			return 0;
		}


};




/*


static int print_advertising_devices(int dd, uint8_t filter_type)
{
//	setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

	if (len < 0)
		return -1;

	return 0;
}
*/

int main()
{
	HCIScanner scanner;

	unsigned char *ptr;
	int len;

	while (1) {
		evt_le_meta_event *meta;
		le_advertising_info *info;
		char addr[18];

		vector<uint8_t> buf = scanner.read_with_retry();
		

		ptr = buf.data() + (1 + HCI_EVENT_HDR_SIZE);
		len -= (1 + HCI_EVENT_HDR_SIZE);

		meta = (evt_le_meta_event*)(void *) ptr;

		if (meta->subevent != 0x02)
			goto done;

		/* Ignoring multiple reports */
		info = (le_advertising_info *) (meta->data + 1);
			ba2str(&info->bdaddr, addr);
			printf("%s \n", addr);
	}

done:;

}
