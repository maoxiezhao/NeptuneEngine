#include "guid.h"

namespace VulkanTest 
{
	Guid Guid::Empty;

	String Guid::ToString(FormatType format) const
	{
        switch (format)
        {
        case FormatType::N:
            return StaticString<MAX_PATH_LENGTH>().Sprintf("%08x%08x%08x%08x", A, B, C, D).c_str();
        case FormatType::B:
            return StaticString<MAX_PATH_LENGTH>().Sprintf("%08x-%04x-%04x-%04x-%04x%08x", A, B >> 16, B & 0xFFFF, C >> 16, C & 0xFFFF, D).c_str();
        case FormatType::P:
            return StaticString<MAX_PATH_LENGTH>().Sprintf("%08x-%04x-%04x-%04x-%04x%08x", A, B >> 16, B & 0xFFFF, C >> 16, C & 0xFFFF, D).c_str();
        default:
            return StaticString<MAX_PATH_LENGTH>().Sprintf("%08x-%04x-%04x-%04x-%04x%08x", A, B >> 16, B & 0xFFFF, C >> 16, C & 0xFFFF, D).c_str();
        }
	}

}