#pragma once

#include "core\common.h"

namespace VulkanTest {

	struct VULKAN_TEST_API StringID 
	{
		static StringID FromU64(U64 hash);

		explicit StringID(const char* string);
		StringID(const void* data, U32 len);
		StringID() {}

		bool operator != (StringID rhs) const { return hash != rhs.hash; }
		bool operator == (StringID rhs) const { return hash == rhs.hash; }
		explicit operator bool()const { return hash != 0; }

		U64 GetHashValue() const { return hash; }

		static const StringID EMPTY;
	private:
		U64 hash = 0;
	};

#define STRING_ID(key) (StringID(#key))
}