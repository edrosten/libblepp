#include "blepp/lescan.h"
#include "blepp/bleclienttransport.h"
#include "blepp/pretty_printers.h"
#include "blepp/gap.h"

#include <string>
#include <cstring>
#include <cerrno>
#include <iomanip>

#ifdef BLEPP_BLUEZ_SUPPORT
#include <bluetooth/hci_lib.h>
#endif

// HCI packet types (standard Bluetooth HCI constants)
#ifndef HCI_EVENT_PKT
#define HCI_EVENT_PKT 0x04
#endif

// HCI event codes (standard Bluetooth HCI constants)
#ifndef EVT_LE_META_EVENT
#define EVT_LE_META_EVENT 0x3E
#endif

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

	AdvertisingResponse::Flags::Flags(std::vector<uint8_t>&& s)
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

	std::string to_hex(const Span& s)
	{
		return to_hex(s.data(), s.size());
	}

	// HCI Scanner error implementation - always available
	HCIScannerError::HCIScannerError(const std::string& why)
	:std::runtime_error(why)
	{
		LOG(LogLevels::Error, why);
	}

	// ===================================================================
	// BLEScanner - Transport-agnostic scanner implementation
	// ===================================================================

	BLEScanner::FilterEntry::FilterEntry(const AdvertisingResponse& ad)
	:mac_address(ad.address), type(static_cast<int>(ad.type))
	{
	}

	bool BLEScanner::FilterEntry::operator<(const FilterEntry& e) const
	{
		if(mac_address < e.mac_address)
			return true;
		else if(mac_address > e.mac_address)
			return false;
		else
			return type < e.type;
	}

	BLEScanner::BLEScanner(BLEClientTransport* transport)
	: transport_(transport)
	, running_(false)
	, filter_mode_(FilterDuplicates::Off)
	{
		if (!transport_) {
			throw std::invalid_argument("BLEScanner: transport cannot be null");
		}
	}

	BLEScanner::~BLEScanner()
	{
		if (running_) {
			try {
				stop();
			} catch (...) {
				// Suppress exceptions in destructor
			}
		}
	}

	void BLEScanner::start(const ScanParams& params)
	{
		ENTER();
		if (running_) {
			LOG(Trace, "Scanner is already running");
			return;
		}

		// Store the filter mode from params for use in get_advertisements()
		filter_mode_ = params.filter_duplicates;

		int result = transport_->start_scan(params);
		if (result < 0) {
			throw HCIScannerError("Failed to start scan");
		}

		scanned_devices_.clear();
		running_ = true;
		LOG(Info, "BLE scanner started");
	}

	void BLEScanner::start(bool passive)
	{
		ScanParams params;
		params.scan_type = passive ? ScanParams::ScanType::Passive : ScanParams::ScanType::Active;
		params.interval_ms = 500;  // 500ms for WiFi coexistence
		params.window_ms = 150;      // 10% duty cycle (~50ms)
		params.filter_duplicates = filter_mode_;  // Use constructor-specified filter mode
		start(params);
	}

	void BLEScanner::stop()
	{
		ENTER();
		if (!running_) {
			return;
		}

		int result = transport_->stop_scan();
		if (result < 0) {
			throw HCIScannerError("Failed to stop scan");
		}

		running_ = false;
		LOG(Info, "BLE scanner stopped");
	}

	std::vector<AdvertisingResponse> BLEScanner::get_advertisements(int timeout_ms)
	{
		if (!running_) {
			throw HCIScannerError("Scanner not running");
		}

		// Get advertisements from transport
		std::vector<AdvertisementData> ads;
		int result = transport_->get_advertisements(ads, timeout_ms);
		if (result < 0) {
			throw HCIScannerError("Failed to get advertisements");
		}

		// Convert AdvertisementData to AdvertisingResponse
		std::vector<AdvertisingResponse> responses;
		for (const auto& ad : ads) {
			AdvertisingResponse resp;
			resp.address = ad.address;
			resp.type = static_cast<LeAdvertisingEventType>(ad.event_type);
			resp.rssi = ad.rssi;

			// Store raw packet data for later parsing if needed
			// The transport's AdvertisementData.data contains the raw advertising payload
			// Applications can parse this using parse_advertisement_packet() if needed
			resp.raw_packet.push_back(ad.data);

			// Software filtering if enabled
			if (filter_mode_ == FilterDuplicates::Software) {
				FilterEntry entry(resp);
				if (scanned_devices_.count(entry)) {
					continue;  // Skip duplicate
				}
				scanned_devices_.insert(entry);
			}

			responses.push_back(std::move(resp));
		}

		return responses;
	}


	// Advertisement packet parsing - available for all transports
	// These functions parse HCI advertisement packets and are used by
	// BLEScanner with any transport backend (BlueZ, Nimble, etc.)

	// Forward declarations for internal parsing functions
	std::vector<AdvertisingResponse> parse_event_packet(Span packet);
	std::vector<AdvertisingResponse> parse_le_meta_event(Span packet);
	std::vector<AdvertisingResponse> parse_le_meta_event_advertisement(Span packet);

	// Standalone function
	std::vector<AdvertisingResponse> parse_advertisement_packet(const std::vector<uint8_t>& p)
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
			throw HCIParseError("Unknown HCI packet received");
		}
	}

	std::vector<AdvertisingResponse> parse_event_packet(Span packet)
	{
		if(packet.size() < 2)
			throw HCIParseError("Truncated event packet");

		uint8_t event_code = packet.pop_front();
		uint8_t length = packet.pop_front();


		if(packet.size() != length)
			throw HCIParseError("Bad packet length");

		if(event_code == EVT_LE_META_EVENT)
		{
			LOG(Info, "event_code = 0x" << std::hex << (int)event_code << ": Meta event" << std::dec);
			LOGVAR(Info, length);

			return parse_le_meta_event(packet);
		}
		else
		{
			LOG(Info, "event_code = 0x" << std::hex << (int)event_code << std::dec);
			LOGVAR(Info, length);
			throw HCIParseError("Unexpected HCI event packet");
		}
	}


	std::vector<AdvertisingResponse> parse_le_meta_event(Span packet)
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

	std::vector<AdvertisingResponse> parse_le_meta_event_advertisement(Span packet)
	{
		std::vector<AdvertisingResponse> ret;

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
				LOG(Warning, "event_type = 0x" << std::hex << (int)event_type << std::dec << ", unknown");
			
			uint8_t address_type = packet.pop_front();

			if(address_type == 0)
				LOG(Info, "Address type = 0: Public device address");
			else if(address_type == 1)
				LOG(Info, "Address type = 0: Random device address");
			else
				LOG(Info, "Address type = 0x" << to_hex(address_type) << ": unknown");


			std::string address;
			for(int j=0; j < 6; j++)
			{
				std::ostringstream s;
				s << std::hex << std::setw(2) << std::setfill('0') << (int) packet.pop_front();
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
				rsp.raw_packet.push_back({data.begin(), data.end()});

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
						rsp.flags = new AdvertisingResponse::Flags({chunk.begin(), chunk.end()});

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
						AdvertisingResponse::Name* n = new AdvertisingResponse::Name();
						n->complete = type==GAP::complete_local_name;
						n->name = std::string(chunk.begin(), chunk.end());
						rsp.local_name = n;

						LOG(Info, "Name (" << (n->complete?"complete":"incomplete") << "): " << n->name);
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
			catch(std::out_of_range& r)
			{
				LOG(LogLevels::Error, "Corrupted data sent by device " << address);
			}
		}

		return ret;
	}

} // namespace BLEPP
