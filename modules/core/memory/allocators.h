#pragma once

#include "allocator.h"
#include "core\platform\sync.h"

namespace VulkanTest
{
	class VULKAN_TEST_API DefaultAllocator final : IAllocator
	{
	public:
		DefaultAllocator();
		~DefaultAllocator();

#ifdef VULKAN_MEMORY_TRACKER
		void* Allocate(size_t size, const char* filename, int line)override;
		void* Reallocate(void* ptr, size_t newBytes, const char* filename, int line)override;
		void  Free(void* ptr)override;
		void* AllocateAligned(size_t size, size_t align, const char* filename, int line)override;
		void* ReallocateAligned(void* ptr, size_t newBytes, size_t align, const char* filename, int line)override;
		void  FreeAligned(void* ptr)override;
#else
		void* Allocate(size_t size)override;
		void* Reallocate(void* ptr, size_t newSize)override;
		void  Free(void* ptr)override;
		void* AllocateAligned(size_t size, size_t align)override;
		void* ReallocateAligned(void* ptr, size_t newSize, size_t align)override;
		void  FreeAligned(void* ptr)override;
#endif
		size_t GetMaxAllocationSize()override;

	public:
		U32 pageCount = 0;
		Mutex mutex;
		U8* smallAllocation = nullptr;

		struct MemPage;
		MemPage* freeList[4];
	};
}