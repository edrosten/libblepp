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

int main()
{
	HCIScanner scanner;
	fd_set fds;
	
	signal(SIGINT, catch_function);

	string throbber="/|\\-";
	
	//hide cursor
	cout << "[?25l" << flush;

	int i=0;
	while (1) {

		struct timeval timeout;     
		timeout.tv_sec = 0;     
		timeout.tv_usec = 300000;

		FD_ZERO(&fds);
		FD_SET(scanner.get_fd(), &fds);
		int err = select(scanner.get_fd()+1, &fds, NULL, NULL,  &timeout);

		if(err < 0 && errno == EINTR)	
			break;
		
		if(FD_ISSET(scanner.get_fd(), &fds))
		{
			vector<AdvertisingResponse> ads = scanner.get_advertisements();


			for(const auto& ad: ads)
			{
				cout << "Found device: " << ad.address << endl;
				for(const auto& uuid: ad.UUIDs)
					cout << "  Service: " << to_str(uuid) << endl;
				if(ad.local_name)
					cout << "  Name: " << ad.local_name->name << endl;
			}
		}
		else
			cout << throbber[i%4] << "\b" << flush;
		i++;
	}

	//show cursor
	cout << "[?25h" << flush;
}
