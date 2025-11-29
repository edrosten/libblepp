#include <blepp/lescan.h>
#include <blepp/bleclienttransport.h>

int main()
{
	BLEPP::log_level = BLEPP::LogLevels::Info;

	// Create transport (auto-selects available transport)
	BLEPP::BLEClientTransport* transport = BLEPP::create_client_transport();
	if (!transport) {
		return 1;
	}

	// Create scanner
	BLEPP::BLEScanner scanner(transport);
	scanner.start();

	while (1) {
		std::vector<BLEPP::AdvertisingResponse> ads = scanner.get_advertisements();
	}

	delete transport;
}
