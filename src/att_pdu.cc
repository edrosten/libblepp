#include <iostream>
#include <libattgatt/att_pdu.h>
#include <libattgatt/pretty_printers.h>

using namespace std;



void pretty_print(const PDUResponse& pdu)
{
	if(log_level >= Debug)
	{
		cerr << "debug: ---PDU packet ---\n";
		cerr << "debug: " << to_hex(pdu.data, pdu.length) << endl;
		cerr << "debug: " << to_str(pdu.data, pdu.length) << endl;
		cerr << "debug: Packet type: " << to_hex(pdu.type()) << " " << att_op2str(pdu.type()) << endl;
		
		if(pdu.type() == ATT_OP_ERROR)
			cerr << "debug: " << PDUErrorResponse(pdu).error_str() << " in response to " <<  att_op2str(PDUErrorResponse(pdu).request_opcode()) << " on handle " + to_hex(PDUErrorResponse(pdu).handle()) << endl;
		else if(pdu.type() == ATT_OP_READ_BY_TYPE_RESP)
		{
			PDUReadByTypeResponse p(pdu);

			cerr << "debug: elements = " << p.num_elements() << endl;
			cerr << "debug: value size = " << p.value_size() << endl;

			for(int i=0; i < p.num_elements(); i++)
			{
				cerr << "debug: " << to_hex(p.handle(i)) << " ";
				if(p.value_size() != 2)
					cerr << "-->" << to_str(p.value(i)) << "<--" << endl;
				else
					cerr << to_hex(p.value_uint16(i)) << endl;
			}

		}
		else if(pdu.type() == ATT_OP_READ_BY_GROUP_RESP)
		{
			PDUReadGroupByTypeResponse p(pdu);
			cerr << "debug: elements = " << p.num_elements() << endl;
			cerr << "debug: value size = " << p.value_size() << endl;

			for(int i=0; i < p.num_elements(); i++)
				cerr << "debug: " <<  "[ " << to_hex(p.start_handle(i)) << ", " << to_hex(p.end_handle(i)) << ") :" << to_str(p.value(i)) << endl;
		}
		else if(pdu.type() == ATT_OP_WRITE_RESP)
		{
		}
		else
			cerr << "debug: --no pretty printer available--\n";
		
		cerr << "debug:\n";
	}
};


