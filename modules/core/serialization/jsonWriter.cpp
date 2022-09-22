#include "jsonWriter.h"

namespace VulkanTest
{
	void JsonWriterBase::Guid(const VulkanTest::Guid& guid)
	{
		// 128bits => 32 chars
		// Binary to hexadecimal
		char buffer[32];
		memset(buffer, 0, sizeof(buffer));
		static const char* digits = "0123456789abcdef";
		I32 offset = 0;
		for (int i = 0; i < 4; i++)
		{
			U32 value = guid.Values[i];
			char* p = buffer + (i * 8 + 7);
			do
			{
				*p-- = digits[value & 0xf];
			} while ((value >>= 4) != 0);
		}
		String(buffer, 32);
	}
}