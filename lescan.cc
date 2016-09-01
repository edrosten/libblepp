#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <cerrno>
#include <array>
#include <vector>

#include <stdexcept>

#include <libattgatt/logging.h>

using namespace std;


class Error: public std::runtime_error 
{
	public:
	Error(const string& why, int errno_val)
	:std::runtime_error(why  + ": " +   strerror(errno_val))
	{
	}
};

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
			throw Error("Setting scan parameters", errno);

		//device disable/enable duplictes ????
		err = hci_le_set_scan_enable(hci_fd, 0x01, filter_dup, 10000);
		if(err < 0)
			throw Error("Enabling scan", errno);

		
		//Set up the filters. 

		socklen_t olen = sizeof(old_filter);
		if (getsockopt(hci_fd, SOL_HCI, HCI_FILTER, &old_filter, &olen) < 0) 
			throw Error("Getting HCI filter socket options", errno);

		
		//Magic incantations to get scan events
		struct hci_filter nf;
		hci_filter_clear(&nf);
		hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
		hci_filter_set_event(EVT_LE_META_EVENT, &nf);
		if (setsockopt(hci_fd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0)
			throw Error("Setting HCI filter socket options", errno);


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
				throw Error("reading HCI packet", errno);
		}

		buf.resize(len);
		return buf;
	}


	private:
		int hci_fd=-1;
		hci_filter old_filter;


		void parse_packet(const vector<uint8_t>& packet)
		{
			if(packet.size() < 1)
			{

			}


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

		//The general HCI event format is (Bluetooth 4.0 Vol 2, Part E,  5.4.4
		// <packet type>
		// <event code>
		// <length>
		// <crap...>
		
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
