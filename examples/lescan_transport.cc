#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <cerrno>
#include <array>
#include <iomanip>
#include <vector>

#include <signal.h>

#include <stdexcept>

#include <blepp/logging.h>
#include <blepp/pretty_printers.h>
#include <blepp/blestatemachine.h> //for UUID. FIXME mofo
#include <blepp/bleclienttransport.h>

using namespace std;
using namespace BLEPP;

void catch_function(int)
{
	cerr << "\nInterrupted!\n";
}

int main(int argc, char** argv)
{
	ScanParams::ScanType type = ScanParams::ScanType::Active;
	bool filter_duplicates = true;
	int c;
	string help = R"X(-[pdhH]:
  -p  passive scan
  -d  show duplicates (no filtering, default is to filter)
  -h  show this message
  -H  use hardware filtering (not supported on all transports)
)X";
	while((c=getopt(argc, argv, "pdhH")) != -1)
	{
		if(c == 'p')
			type = ScanParams::ScanType::Passive;
		else if(c == 'd')
			filter_duplicates = false;
		else if(c == 'H')
		{
			// Hardware filtering - note: may not be supported by all transports
			cerr << "Warning: hardware filtering may not be supported by all transports" << endl;
		}
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

	// Create transport using factory (will select BlueZ or ATBM based on availability)
	BLEClientTransport* transport = create_client_transport();
	if (!transport)
	{
		cerr << "Failed to create BLE client transport. No transports available." << endl;
		return 1;
	}

	cout << "Using transport: " << transport->get_transport_name() << endl;

	// Configure scan parameters
	ScanParams params;
	params.scan_type = type;
	params.filter_duplicates = filter_duplicates;
	params.interval_ms = 10;
	params.window_ms = 10;

	// Start scanning
	if (transport->start_scan(params) < 0)
	{
		cerr << "Failed to start scanning" << endl;
		delete transport;
		return 1;
	}

	//Catch the interrupt signal
	signal(SIGINT, catch_function);

	//Something to print to demonstrate the timeout.
	string throbber="/|\\-";

	//hide cursor, to make the throbber look nicer.
	cout << "[?25l" << flush;

	int i=0;
	while (1)
	{
		// Get advertisements with 300ms timeout
		vector<AdvertisementData> ads;
		int result = transport->get_advertisements(ads, 300);

		//Interrupted, so quit and clean up properly.
		if(result < 0 && errno == EINTR)
			break;

		if (result > 0 && !ads.empty())
		{
			for(const auto& ad: ads)
			{
				cout << "Found device: " << ad.address << " ";

				// Decode event type
				if(ad.event_type == 0x00)
					cout << "Connectable undirected (ADV_IND)" << endl;
				else if(ad.event_type == 0x01)
					cout << "Connectable directed (ADV_DIRECT_IND)" << endl;
				else if(ad.event_type == 0x02)
					cout << "Scannable (ADV_SCAN_IND)" << endl;
				else if(ad.event_type == 0x03)
					cout << "Non connectable (ADV_NONCONN_IND)" << endl;
				else if(ad.event_type == 0x04)
					cout << "Scan response (SCAN_RSP)" << endl;
				else
					cout << "Unknown event type: " << (int)ad.event_type << endl;

				// Parse advertisement data for UUIDs and names
				// TODO: Parse ad.data for UUIDs, local name, etc.
				// For now, just show raw data length
				cout << "  Data length: " << ad.data.size() << " bytes" << endl;

				if(ad.rssi == 127)
					cout << "  RSSI: unavailable" << endl;
				else if(ad.rssi <= 20)
					cout << "  RSSI = " << (int) ad.rssi << " dBm" << endl;
				else
					cout << "  RSSI = " << to_hex((uint8_t)ad.rssi) << " unknown" << endl;
			}
		}
		else
		{
			cout << throbber[i%4] << "\b" << flush;
		}
		i++;
	}

	// Stop scanning and cleanup
	transport->stop_scan();
	delete transport;

	//show cursor
	cout << "[?25h" << flush;

	return 0;
}
