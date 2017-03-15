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
#include <boost/optional.hpp>

#include <signal.h>

#include <stdexcept>

#include <blepp/logging.h>
#include <blepp/pretty_printers.h>
#include <blepp/blestatemachine.h> //for UUID. FIXME mofo
#include <blepp/lescan.h>

using namespace std;
using namespace BLEPP;

void catch_function(int)
{
	cerr << "\nInterrupted!\n";
}

int main(int argc, char** argv)
{
	HCIScanner::ScanType type = HCIScanner::ScanType::Active;
	HCIScanner::FilterDuplicates filter = HCIScanner::FilterDuplicates::Software;
	int c;
	string help = R"X(-[sHbdhp]:
  -s  software filtering of duplicates (default)
  -H  hardware filtering of duplicates 
  -b  both hardware and software filtering
  -d  show duplicates (no filtering)
  -h  show this message
  -p  passive scan
)X";
	while((c=getopt(argc, argv, "sHbdhp")) != -1)
	{
		if(c == 'p')
			type = HCIScanner::ScanType::Passive;
		else if(c == 's')
			filter = HCIScanner::FilterDuplicates::Software;
		else if(c == 'H')
			filter = HCIScanner::FilterDuplicates::Hardware;
		else if(c == 'b')
			filter = HCIScanner::FilterDuplicates::Both;
		else if(c == 'd')
			filter = HCIScanner::FilterDuplicates::Off;
		else if(c == 'h')
		{
			cout << "Usage: " << argv[0] << " " << help;
			return 0;
		}
		else 
		{
			cerr << argv[0] << ":  unknown option " << c << endl;
			return 1;
		}
	}


	log_level = LogLevels::Warning;
	HCIScanner scanner(true, filter, type);
	
	//Catch the interrupt signal. If the scanner is not 
	//cleaned up properly, then it doesn't reset the HCI state.
	signal(SIGINT, catch_function);

	//Something to print to demonstrate the timeout.
	string throbber="/|\\-";
	
	//hide cursor, to make the throbber look nicer.
	cout << "[?25l" << flush;

	int i=0;
	while (1) {
		

		//Check to see if there's anything to read from the HCI
		//and wait if there's not.
		struct timeval timeout;     
		timeout.tv_sec = 0;     
		timeout.tv_usec = 300000;

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(scanner.get_fd(), &fds);
		int err = select(scanner.get_fd()+1, &fds, NULL, NULL,  &timeout);
		
		//Interrupted, so quit and clean up properly.
		if(err < 0 && errno == EINTR)	
			break;
		
		if(FD_ISSET(scanner.get_fd(), &fds))
		{
			//Only read id there's something to read
			vector<AdvertisingResponse> ads = scanner.get_advertisements();

			for(const auto& ad: ads)
			{
				cout << "Found device: " << ad.address << " ";

				if(ad.type == LeAdvertisingEventType::ADV_IND)
					cout << "Connectable undirected" << endl;
				else if(ad.type == LeAdvertisingEventType::ADV_DIRECT_IND)
					cout << "Connectable directed" << endl;
				else if(ad.type == LeAdvertisingEventType::ADV_SCAN_IND)
					cout << "Scannable " << endl;
				else if(ad.type == LeAdvertisingEventType::ADV_NONCONN_IND)
					cout << "Non connectable" << endl;
				else
					cout << "Scan response" << endl;
				for(const auto& uuid: ad.UUIDs)
					cout << "  Service: " << to_str(uuid) << endl;
				if(ad.local_name)
					cout << "  Name: " << ad.local_name->name << endl;
				if(ad.rssi == 127)
					cout << "  RSSI: unavailable" << endl;
				else if(ad.rssi <= 20)
					cout << "  RSSI = " << (int) ad.rssi << " dBm" << endl;
				else
					cout << "  RSSI = " << to_hex((uint8_t)ad.rssi) << " unknown" << endl;
			}
		}
		else
			cout << throbber[i%4] << "\b" << flush;
		i++;
	}

	//show cursor
	cout << "[?25h" << flush;
}
