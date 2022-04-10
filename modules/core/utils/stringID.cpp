#include "stringID.h"
#include "string.h"
#include "math\hash.h"

namespace VulkanTest {

	StringID const StringID::EMPTY = StringID();

	StringID StringID::FromU64(U64 hash)
	{
		StringID ret;
		ret.hash = hash;
		return ret;
	}

	StringID::StringID(const char* str)
	{
		hash = XXHash64(str, StringLength(str));
	}

	StringID::StringID(const void* data, U32 len)
	{
		hash = XXHash64(data, len);
	}
}
