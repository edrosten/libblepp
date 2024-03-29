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

#include <iostream>
#include <blepp/att_pdu.h>
#include <blepp/pretty_printers.h>

void BLEPP::pretty_print(const PDUResponse& pdu)
{
	if(log_level >= Debug)
	{
		std::cerr << "debug: ---PDU packet ---\n";
		std::cerr << "debug: " << to_hex(pdu.data, pdu.length) << std::endl;
		std::cerr << "debug: " << to_str(pdu.data, pdu.length) << std::endl;
		std::cerr << "debug: Packet type: " << to_hex(pdu.type()) << " " << att_op2str(pdu.type()) << std::endl;
		
		if(pdu.type() == ATT_OP_ERROR)
			std::cerr << "debug: " << PDUErrorResponse(pdu).error_str() << " in response to " <<  att_op2str(PDUErrorResponse(pdu).request_opcode()) << " on handle " + to_hex(PDUErrorResponse(pdu).handle()) << std::endl;
		else if(pdu.type() == ATT_OP_READ_BY_TYPE_RESP)
		{
			PDUReadByTypeResponse p(pdu);

			std::cerr << "debug: elements = " << p.num_elements() << std::endl;
			std::cerr << "debug: value size = " << p.value_size() << std::endl;

			for(int i=0; i < p.num_elements(); i++)
			{
				std::cerr << "debug: " << to_hex(p.handle(i)) << " ";
				if(p.value_size() != 2)
					std::cerr << "-->" << to_str(p.value(i)) << "<--" << std::endl;
				else
					std::cerr << to_hex(p.value_uint16(i)) << std::endl;
			}

		}
		else if(pdu.type() == ATT_OP_READ_BY_GROUP_RESP)
		{
			PDUReadGroupByTypeResponse p(pdu);
			std::cerr << "debug: elements = " << p.num_elements() << std::endl;
			std::cerr << "debug: value size = " << p.value_size() << std::endl;

			for(int i=0; i < p.num_elements(); i++)
				std::cerr << "debug: " <<  "[ " << to_hex(p.start_handle(i)) << ", " << to_hex(p.end_handle(i)) << ") :" << to_str(p.value(i)) << std::endl;
		}
		else if(pdu.type() == ATT_OP_WRITE_RESP)
		{
		}
		else if(pdu.type() == ATT_OP_HANDLE_NOTIFY || pdu.type() == ATT_OP_HANDLE_IND)
		{
			PDUNotificationOrIndication p(pdu);
			std::cerr << "debug: handle = " << p.handle() << std::endl;
			std::cerr << "debug: data = " << to_hex(p.value().first, p.value().second - p.value().first) << std::endl;
			std::cerr << "debug: data = " << to_str(p.value().first, p.value().second - p.value().first) << std::endl;

		}
		else
			std::cerr << "debug: --no pretty printer available--\n";
		
		std::cerr << "debug:\n";
	}
};


