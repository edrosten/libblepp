#include <blepp/float.h>
#include <cmath>

using namespace std;

namespace BLEPP
{

	float bluetooth_float_to_IEEE754(const uint8_t* bytes)
	{
		int exponent = (int8_t)bytes[3];

		int mantissa = (bytes[0]) | (bytes[1] << 8) | (bytes[2] << 16);

		if(mantissa > 0x080000)
			mantissa = - (0x100000-mantissa);


		return mantissa * pow(10, exponent);

	}
}
