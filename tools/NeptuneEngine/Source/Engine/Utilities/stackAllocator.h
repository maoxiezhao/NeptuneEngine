#pragma once

#include <algorithm>

namespace VulkanTest
{
namespace Util
{
	template<typename T, size_t N>
	class StackAllocator
	{
	public:
		T* AllocateMem(size_t count = 1)
		{
			if (count == 0 || offset + count > N)
				return nullptr;

			T* ret = buffer + offset;
			offset += count;
			return ret;
		}

		T* Allocate(size_t count = 1)
		{
			T* mem = AllocateMem(count);
			if (mem != nullptr)
				std::fill(mem, mem + count, T());
			return mem;
		}

		void Reset()
		{
			offset = 0;
		}

	private:
		T buffer[N];
		size_t offset = 0;
	};
}
}