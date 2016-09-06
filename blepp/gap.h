#ifndef BLEPLUSPLUS_INC_GAP_H
#define BLEPLUSPLUS_INC_GAP_H

namespace BLEPP
{
	namespace GAP
	{
		//From here:
		//https://www.bluetooth.org/en-us/specification/assigned-numbers/generic-access-profile
		//incomplete list.
		enum GAP
		{
			flags = 0x01,
			incomplete_list_of_16_bit_UUIDs = 0x02,
			complete_list_of_16_bit_UUIDs = 0x03,
			incomplete_list_of_128_bit_UUIDs = 0x06,
			complete_list_of_128_bit_UUIDs = 0x07,
			shortened_local_name = 0x08,
			complete_local_name = 0x09,
			service_data_16_bit_UUID = 0x16,
			manufacturer_data = 0xff
		};

	};



}


#endif
