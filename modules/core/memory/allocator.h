#pragma once

#include "core\common.h"

///////////////////////////////////////////////////////////////////////
// allocator definitions
#define VULKAN_MEMORY_ALLOCATOR_DEFAULT 1

///////////////////////////////////////////////////////////////////////
// memory tracker
#ifdef DEBUG
#define VULKAN_MEMORY_TRACKER
#endif

///////////////////////////////////////////////////////////////////////
// common memory allocator
#define VULKAN_MEMORY_ALLOCATOR	VULKAN_MEMORY_ALLOCATOR_DEFAULT

// container allocator
#define VULKAN_CONTAINER_ALLOCATOR  VULKAN_MEMORY_ALLOCATOR_DEFAULT

///////////////////////////////////////////////////////////////////////
// use stl smart pointer
#define USE_STL_SMART_POINTER

namespace VulkanTest
{
	class VULKAN_TEST_API IAllocator
	{
	public:
		IAllocator() {};
		virtual ~IAllocator() {};

#ifdef VULKAN_MEMORY_TRACKER
		virtual void* Allocate(size_t size, const char* filename, int line) = 0;
		virtual void* Reallocate(void* ptr, size_t newBytes, const char* filename, int line) = 0;
		virtual void  Free(void* ptr) = 0;
		virtual void* AllocateAligned(size_t size, size_t align, const char* filename, int line) = 0;
		virtual void* ReallocateAligned(void* ptr, size_t newBytes, size_t align, const char* filename, int line) = 0;
		virtual void  FreeAligned(void* ptr) = 0;
#else
		virtual void* Allocate(size_t size) = 0;
		virtual void* Reallocate(void* ptr, size_t newSize) = 0;
		virtual void  Free(void* ptr) = 0;
		virtual void* AlignAllocate(size_t size, size_t align) = 0;
		virtual void* AlignReallocate(void* ptr, size_t newSize, size_t align) = 0;
		virtual void  AlignFree(void* ptr) = 0;
#endif

		// Get the maximum size of a single allocation
		virtual size_t GetMaxAllocationSize() = 0;

		template<typename T, typename... Args>
		T* New(Args&&... args)
		{
			void* mem = Allocate(sizeof(T), alignof(T));
			return new(mem) T(std::forward<Args>(args)...);
		}

		template<typename T>
		void Delete(T* obj)
		{
			if (obj != nullptr)
			{
				if (!__has_trivial_destructor(T)) {
					obj->~T();
				}
				Free(obj);
			}
		}
	};
}