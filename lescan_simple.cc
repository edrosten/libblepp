#include <blepp/lescan.h>

int main()
{
	BLEPP::log_level = BLEPP::LogLevels::Info;
	BLEPP::HCIScanner scanner;
	while (1) {
		std::vector<BLEPP::AdvertisingResponse> ads = scanner.get_advertisements();
	}
}
