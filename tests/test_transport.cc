#include <blepp/bleclienttransport.h>
#include <blepp/logging.h>
#include <iostream>
#include <cstdlib>

using namespace BLEPP;

#define check(X) do{\
if(!(X))\
{\
	std::cerr << "Test failed on line " << __LINE__ << ": " << #X << std::endl;\
	exit(1);\
}}while(0)

int main()
{
	log_level = LogLevels::Warning;

	// Test 1: Factory function may return nullptr if no hardware is available
	// This is OK in build environments - we test what we can
	BLEClientTransport* transport = create_client_transport();

	if (transport == nullptr) {
		// No transport available (no hardware/permissions)
		// This is acceptable in build/test environments
		std::cout << "No transport available (no hardware/permissions) - skipping runtime tests" << std::endl;
		std::cout << "OK" << std::endl;
		return 0;
	}

	// Test 2: Transport should have a name
	const char* name = transport->get_transport_name();
	check(name != nullptr);
	check(name[0] != '\0');
	std::cout << "Using transport: " << name << std::endl;

	// Test 3: Transport should report as available (if factory returned it)
	check(transport->is_available());

	// Test 4: Default MTU should be valid (23 is BLE minimum)
	// Note: Can't test with invalid fd=-1 on all transports, so skip this test
	// uint16_t mtu = transport->get_mtu(-1);
	// check(mtu >= 23);

	// Test 5: ScanParams should have sensible defaults
	ScanParams params;
	check(params.scan_type == ScanParams::ScanType::Active);
	check(params.interval_ms == 10);
	check(params.window_ms == 10);
	check(params.filter_policy == ScanParams::FilterPolicy::All);
	check(params.filter_duplicates == true);

	// Test 6: Can configure scan parameters
	params.scan_type = ScanParams::ScanType::Passive;
	params.filter_duplicates = false;
	check(params.scan_type == ScanParams::ScanType::Passive);
	check(params.filter_duplicates == false);

	// Cleanup
	delete transport;

	std::cout << "OK" << std::endl;
	return 0;
}
