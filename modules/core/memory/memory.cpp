#include "memory.h"

namespace VulkanTest
{
	MemoryAllocator allocator;

#ifdef VULKAN_MEMORY_TRACKER
	void* Memory::Alloc(size_t size, const char* filename, int line)
	{
		return allocator.Allocate(size, filename, line);
	}

	void* Memory::Realloc(void* ptr, size_t newBytes, const char* filename, int line)
	{
		return allocator.Reallocate(ptr, newBytes, filename, line);
	}

	void Memory::Free(void* ptr)
	{
		allocator.Free(ptr);
	}

	void* Memory::AllocAligned(size_t size, size_t align, const char* filename, int line)
	{
		return allocator.AllocateAligned(size, align, filename, line);
	}

	void* Memory::ReallocAligned(void* ptr, size_t newBytes, size_t align, const char* filename, int line)
	{
		return allocator.ReallocateAligned(ptr, newBytes, align, filename, line);
	}

	void Memory::FreeAligned(void* ptr)
	{
		allocator.FreeAligned(ptr);
	}

#else
	void* Memory::Alloc(size_t size)
	{
		return allocator.Allocate(size);
	}

	void* Memory::Realloc(void* ptr, size_t newBytes)
	{
		return allocator.Reallocate(ptr, newBytes);
	}

	void Memory::Free(void* ptr)
	{
		return allocator.Free(ptr);
	}

	void* Memory::AllocAligned(size_t size, size_t align)
	{
		return allocator.AllocateAligned(size, align);
	}

	void* Memory::ReallocAligned(void* ptr, size_t newBytes, size_t align)
	{
		return allocator.ReallocateAligned(ptr, newBytes, align);
	}

	void Memory::FreeAligned(void* ptr)
	{
		return allocator.FreeAligned(ptr);
	}
#endif

	void Memory::Memmove(void* dst, const void* src, size_t size)
	{
		memmove_s(dst, size, src, size);
	}

	void Memory::Memcpy(void* dst, const void* src, size_t size)
	{
		memcpy_s(dst, size, src, size);
	}

	void Memory::Memset(void* dst, int c, int count)
	{
		memset(dst, c, count);
	}
}