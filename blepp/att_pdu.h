/*
 *
 *  blepp - Implementation of the Generic ATTribute Protocol
 *
 *  Copyright (C) 2013, 2014 Edward Rosten
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


/*
	This file contains classes to wrap ATT Protocol (Attribute Protocol) PDUs
	(Protocol Data Units?) and present the data in a semantically meaningful
	format.

	The format of the packets is covered in Core Spec 4.0 3.F.3.4

	Logic errors (such as trying to construct a PDU packet class from the 
	wrong PDU data) come back as std::logic_error. Runtime errors, such as
	invalid packets come back as exceptions derived from std::runtime_error.

*/

#ifndef INCLUDE_BLUETOOTH_ATT_H_C51BA176654792D689D16398C9A4735A
#define INCLUDE_BLUETOOTH_ATT_H_C51BA176654792D689D16398C9A4735A

#include <cassert>
#include <stdexcept>
#include <utility>

#include <blepp/att.h>
#include <blepp/logging.h>

namespace BLEPP
{

	/* Basic PDU response as in 3.F.3.3.1 */
	class PDUResponse
	{
		protected:

			template<class C>
			void error(const std::string& s) const
			{
				LOG(Error, s);
				throw C(s);
			}

			void type_check(int target) const
			{	
				if(target != type())
					error<std::logic_error>(std::string("Error converting PDUResponse to ") +att_op2str(target)  + ". Type is " + att_op2str(type()));
			}

		public:

			const uint8_t* data; //Pointer to the underlying data
			int length;          //Length of the underlying data

			//Utility function to return a uint8 at byte offset i;
			uint8_t uint8(int i) const
			{
				assert(i >= 0 && i < length);
				return data[i];
			}

			//Utility function to return a uint16 at byte offset i
			//Note packets are little endian.
			uint16_t uint16(int i) const
			{
				return uint8(i) | (uint8(i+1) << 8);
			}

			PDUResponse(const uint8_t* d_, int l_)
				:data(d_),length(l_)
			{
			}

			//Table 3.1: type field is the low 6 bits.
			//The command flag is bit 6 and the authentication
			//flag is bit 7.

			//Return the method type
			uint8_t type() const 
			{
				return uint8(0) & 63;
			}

			bool is_command() const
			{
				return uint8(0) & 64;
			}

			bool is_authenticated() const
			{
				return uint8(0) & 128;
			}
	};

	/* Error packet response 3.F.3.4.1.1 */
	class PDUErrorResponse: public PDUResponse
	{
		public:
			PDUErrorResponse(const PDUResponse& p_)
				:PDUResponse(p_)
			{
				type_check(ATT_OP_ERROR);
			}

			uint8_t request_opcode() const
			{
				return uint8(1);
			}

			uint16_t handle() const
			{
				return uint16(2);
			}

			uint8_t error_code() const
			{
				return uint8(4);
			}

			const char* error_str() const
			{
				return att_ecode2str(error_code());
			}
	};
	/* Response to read_req, 3.F.3.4.4.4 */
	class PDUReadResponse: public PDUResponse
	{
		public:
			PDUReadResponse(const PDUResponse& p_)
			:PDUResponse(p_)
			{
				type_check(ATT_OP_READ_RESP);
			}
			uint8_t request_opcode() const
			{
				return uint8(1);
			}

			int num_elements() const
			{
				return length-1;
			}

			std::pair<const uint8_t*, const uint8_t*> value() const
			{
				return std::make_pair(data + 1, data + length);
			}
	};



	/* Response to read_by_type, 3.F.3.4.4.2 */
	class PDUReadByTypeResponse: public PDUResponse
	{
		public:
			PDUReadByTypeResponse(const PDUResponse& p_)
			:PDUResponse(p_)
			{
				type_check(ATT_OP_READ_BY_TYPE_RESP);

				if((length - 2) % element_size() != 0)
					error<std::runtime_error>("Invalid packet length for PDUReadByTypeResponse");
			}


			//Size of each element in the response
			int element_size() const
			{
				return uint8(1);
			}

			//Elements consist of a handle and a value.
			//This is the size of just the value.
			int value_size() const
			{
				return uint8(1) -2;
			}

			int num_elements() const
			{
				return (length - 1) / element_size();
			}

			uint16_t handle(int i) const
			{
				return uint16(i*element_size() + 2);
			}

			//Return pointer span of the ith chunk of data
			std::pair<const uint8_t*, const uint8_t*> value(int i) const
			{
				const uint8_t* begin = data + i*element_size() + 4;
				return std::make_pair(begin, begin + value_size());
			}

			//Utility function to return the ith element directly as a uint16.
			uint16_t value_uint16(int i) const
			{
				if(value_size() != 2)
					error<std::logic_error>("Wrong size for uint16 in PDUReadByTypeResponse");

				return uint16(i*element_size()+4);
			}
	};


	/* Response to read_group_by_type, 3.F.3.4.4.10 */
	class PDUReadGroupByTypeResponse: public PDUResponse
	{
		public:
			PDUReadGroupByTypeResponse(const PDUResponse& p_)
			:PDUResponse(p_)
			{
				type_check(ATT_OP_READ_BY_GROUP_RESP);

				if((length - 2) % element_size() != 0)
					error<std::runtime_error>("Invalid packet length for PDUReadGroupByTypeResponse");

			}

			int value_size() const
			{
				return uint8(1) -4;
			}

			int element_size() const
			{
				return uint8(1);
			}

			int num_elements() const
			{
				return (length - 2) / element_size();
			}

			uint16_t start_handle(int i) const
			{
				return uint16(i*element_size() + 2);
			}

			uint16_t end_handle(int i) const
			{
				return uint16(i*element_size() + 4);
			}

			std::pair<const uint8_t*, const uint8_t*> value(int i) const
			{
				const uint8_t* begin = data + i*element_size() + 6;
				return std::make_pair(begin, begin + value_size());
			}

			uint16_t value_uint16(int i) const
			{
				if(value_size() != 2)
					error<std::logic_error>("Wrong size for uint16 in PDUReadGroupByTypeResponse");
				return uint16(i*element_size()+4);
			}

	};

	class PDUFindInformationResponse: public PDUResponse
	{
		public:

			PDUFindInformationResponse(const PDUResponse& p_)
			:PDUResponse(p_)
			{
				type_check(ATT_OP_FIND_INFO_RESP);
				if( (length-2) % element_size())
					error<std::runtime_error>("Invalid packet length for PDUReadGroupByTypeResponse");
			}

			bool is_16_bit() const
			{
				//Table 3.8
				return uint8(1) == 1;

			};

			int element_size() const
			{
				return 2 + (is_16_bit() ? 2 : 16);
			}

			int num_elements() const
			{
				return (length - 2) / element_size();
			}

			uint16_t handle(int i) const
			{
				return uint16(2 + i * element_size());
			}

			bt_uuid_t uuid(int i) const
			{
				if(is_16_bit())
					return att_get_uuid16(data + 2 + 2 + i * element_size());
				else
					return att_get_uuid128(data + 2 + 2 + i * element_size());
			}
	};

	class PDUNotificationOrIndication: public PDUResponse
	{
		public:

			PDUNotificationOrIndication(const PDUResponse& p_)
			:PDUResponse(p_)
			{
				if(type() != ATT_OP_HANDLE_NOTIFY && type() != ATT_OP_HANDLE_IND)
					error<std::logic_error>(std::string("Error converting PDUResponse to NotifyOrIndicate. Type is ") + att_op2str(type()));
			}

			bool notification() const
			{
				return type() == ATT_OP_HANDLE_NOTIFY;

			};

			int num_elements() const
			{
				return (length - 2);
			}

			uint16_t handle() const
			{
				return uint16(1);
			}

			//Return pointer span of the ith chunk of data
			std::pair<const uint8_t*, const uint8_t*> value() const
			{
				return std::make_pair(data + 3, data + length);
			}
	};

	void pretty_print(const PDUResponse& pdu);
}


#endif
