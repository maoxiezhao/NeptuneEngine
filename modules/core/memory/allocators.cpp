#include "allocators.h"
#include "memTracker.h"
#include "memory.h"
#include "platform\platform.h"

namespace VulkanTest
{
	// Small alloc strategy:
	// MAX_SMALL_SIZE is 64
	// According to size divided into 4 free lists: 8 16 32 64

	static constexpr U32 SMALL_ALLOC_MAX_SIZE = 64;
	static constexpr U32 PAGE_SIZE = 4096;
	static constexpr size_t MAX_PAGE_COUNT = 16384;

	struct DefaultAllocator::MemPage
	{
		struct Header
		{
			MemPage* prev;
			MemPage* next;
			U32 firstFree;
			U32 itemSize;
		};
		U8 data[PAGE_SIZE - sizeof(Header)];
		Header header;
	};

	static U32 GetFreeListIndex(size_t size)
	{
		// According to size divided into 4 free lists: 8 16 32 64
		// (0, 8]   => 0; 
		// (8, 16]  => 1; 
		// (16, 32] => 2; 
		// (32, 64] => 3; 
		ASSERT(size > 0);
		ASSERT(size <= SMALL_ALLOC_MAX_SIZE);
#ifdef _WIN32
		unsigned long res;
		return _BitScanReverse(&res, ((unsigned long)size - 1) >> 2) ? res : 0;
#else
		return 31 - __builtin_clz((n - 1) >> 2);
#endif
	}

	static void InitMemPage(DefaultAllocator::MemPage* page, int itemSize)
	{
		Platform::MemCommit(page, PAGE_SIZE);
		page = new (NewPlaceHolder(), page) DefaultAllocator::MemPage;
		page->header.prev = nullptr;
		page->header.next = nullptr;
		page->header.firstFree = 0;
		page->header.itemSize = itemSize;

		for (int i = 0; i < (sizeof(page->data) / itemSize); i++)
			*(U32*)&page->data[i * itemSize] = U32(i * itemSize + itemSize);
	}

	static DefaultAllocator::MemPage* GetMemPage(void* ptr)
	{
		// Page mem block:
		// | BaseAddr| PAGE_SIZE |
		return (DefaultAllocator::MemPage*)((UIntPtr)ptr & ~(U64)(PAGE_SIZE - 1));
	}

	static void* AllocSmall(DefaultAllocator& allocator, size_t size)
	{
		U32 freeListIndex = GetFreeListIndex(size);
		ScopedMutex lock(allocator.mutex);
		if (allocator.smallAllocation == nullptr)
			allocator.smallAllocation = (U8*)Platform::MemReserve(MAX_PAGE_COUNT * PAGE_SIZE);

		DefaultAllocator::MemPage* page = allocator.freeList[freeListIndex];
		if (page == nullptr)
		{
			if (allocator.pageCount == MAX_PAGE_COUNT)
				return nullptr;

			page = (DefaultAllocator::MemPage*)(allocator.smallAllocation + PAGE_SIZE * allocator.pageCount);
			InitMemPage(page, 8 << freeListIndex);
			allocator.freeList[freeListIndex] = page;
			allocator.pageCount++;
		}

		ASSERT(page->header.itemSize > 0);
		ASSERT(page->header.firstFree + size < sizeof(page->data));
		void* mem = &page->data[page->header.firstFree];
		page->header.firstFree = *(U32*)mem;

		if (page->header.firstFree + page->header.itemSize > sizeof(page->data))
		{
			// If the page is full (dose not have enough mem for next allocation)
			// Set freelist is null, it will reallocate a new page
			if (allocator.freeList[freeListIndex] == page)
				allocator.freeList[freeListIndex] = page->header.next;

			if (page->header.next != nullptr)
				page->header.next->header.prev = page->header.prev;
			if (page->header.prev != nullptr)
				page->header.prev->header.next = page->header.next;

			page->header.prev = page->header.next = nullptr;
		}

		return mem;
	}

	static void FreeSmall(DefaultAllocator& allocator, void* mem)
	{
		DefaultAllocator::MemPage* page = GetMemPage(mem);
		ScopedMutex lock(allocator.mutex);

		if (page->header.firstFree + page->header.itemSize > sizeof(page->data))
		{
			ASSERT(!page->header.next);
			ASSERT(!page->header.prev);

			U32 freeListIndex = GetFreeListIndex(page->header.itemSize);
			page->header.next = allocator.freeList[freeListIndex];
			allocator.freeList[freeListIndex] = page;
		}

		*(U32*)mem = page->header.firstFree;
		page->header.firstFree = U32((U8*)mem - page->data);
	}

	static void* ReallocSmall(DefaultAllocator& allocator, void* mem, size_t size)
	{
		// Check itemSize
		DefaultAllocator::MemPage* page = GetMemPage(mem);
		if (size <= SMALL_ALLOC_MAX_SIZE)
		{
			U32 i = GetFreeListIndex(size);
			DefaultAllocator::MemPage* newPage = allocator.freeList[i];
			if (newPage != nullptr && newPage->header.itemSize == page->header.itemSize)
				return mem;
		}

		void* newMem = size <= SMALL_ALLOC_MAX_SIZE ? AllocSmall(allocator, size) : malloc(size);
		memcpy(newMem, mem, std::min((size_t)page->header.itemSize, size));
		FreeSmall(allocator, mem);
		return newMem;
	}

	static void* ReallocSmallAligned(DefaultAllocator& allocator, void* mem, size_t size, size_t align)
	{
		// Check itemSize
		DefaultAllocator::MemPage* page = GetMemPage(mem);
		if (size <= SMALL_ALLOC_MAX_SIZE)
		{
			U32 i = GetFreeListIndex(size);
			if (allocator.freeList[i]->header.itemSize == page->header.itemSize)
				return mem;
		}
		void* newMem = size <= SMALL_ALLOC_MAX_SIZE ? AllocSmall(allocator, size) : _aligned_malloc(size, align);
		memcpy(newMem, mem, std::min((size_t)page->header.itemSize, size));
		FreeSmall(allocator, mem);
		return newMem;
	}

	static bool IsSmallAlloc(DefaultAllocator& allocator, void* mem)
	{
		return allocator.smallAllocation != nullptr &&
			mem >= allocator.smallAllocation &&
			mem < (allocator.smallAllocation + MAX_PAGE_COUNT * PAGE_SIZE);
	}

	DefaultAllocator::DefaultAllocator()
	{
		pageCount = 0;
		memset(freeList, 0, sizeof(freeList));
	}

	DefaultAllocator::~DefaultAllocator()
	{
		if (smallAllocation != nullptr)
			Platform::MemRelease(smallAllocation, MAX_PAGE_COUNT * PAGE_SIZE);
	}

#ifdef VULKAN_MEMORY_TRACKER
	void* DefaultAllocator::Allocate(size_t size, const char* filename, int line)
	{
		void* ptr = size <= SMALL_ALLOC_MAX_SIZE ? AllocSmall(*this, size) : malloc(size);
		MemoryTracker::Get().RecordAlloc(ptr, size, filename, line);
		return ptr;
	}

	void* DefaultAllocator::Allocate(size_t size)
	{
		return size <= SMALL_ALLOC_MAX_SIZE ? AllocSmall(*this, size) : malloc(size);
	}

	void* DefaultAllocator::Reallocate(void* ptr, size_t newBytes, const char* filename, int line)
	{
		void* ret = IsSmallAlloc(*this, ptr) ? ReallocSmall(*this, ptr, newBytes) : realloc(ptr, newBytes);
		MemoryTracker::Get().RecordRealloc(ret, ptr, newBytes, filename, line);
		return ret;
	}

	void DefaultAllocator::Free(void* ptr)
	{
		MemoryTracker::Get().RecordFree(ptr);

		if (IsSmallAlloc(*this, ptr))
			FreeSmall(*this, ptr);
		else
			free(ptr);
	}

	void* DefaultAllocator::AllocateAligned(size_t size, size_t align, const char* filename, int line)
	{
		void* ptr = nullptr;
		if (size <= SMALL_ALLOC_MAX_SIZE && align <= size)
			ptr = AllocSmall(*this, size);
		else
			ptr = _aligned_malloc(size, align);

		MemoryTracker::Get().RecordAlloc(ptr, size, filename, line);
		return ptr;
	}

	void* DefaultAllocator::ReallocateAligned(void* ptr, size_t newBytes, size_t align, const char* filename, int line)
	{
		void* ret = IsSmallAlloc(*this, ptr) ? ReallocSmallAligned(*this, ptr, newBytes, align) : _aligned_realloc(ptr, newBytes, align);
		MemoryTracker::Get().RecordRealloc(ret, ptr, newBytes, filename, line);
		return ret;
	}

	void DefaultAllocator::FreeAligned(void* ptr)
	{
		MemoryTracker::Get().RecordFree(ptr);

		if (IsSmallAlloc(*this, ptr))
			FreeSmall(*this, ptr);
		else
			_aligned_free(ptr);
	}

#else
	void* DefaultAllocator::Allocate(size_t size)
	{
		return size <= SMALL_ALLOC_MAX_SIZE ? AllocSmall(*this, size) : malloc(size);
	}

	void* DefaultAllocator::Reallocate(void* ptr, size_t newSize)
	{
		return IsSmallAlloc(*this, ptr) ? ReallocSmall(*this, ptr, newBytes) : realloc(ptr, newBytes);
	}

	void DefaultAllocator::Free(void* ptr)
	{
		if (IsSmallAlloc(*this, ptr))
			FreeSmall(*this, ptr);
		else
			free(ptr);
	}

	void* DefaultAllocator::AllocateAligned(size_t size, size_t align)
	{
		void* ptr = nullptr;
		if (size <= SMALL_ALLOC_MAX_SIZE && align <= size)
			ptr = AllocSmall(*this, size);
		else
			ptr = _aligned_malloc(size, align);
		return ptr;
	}

	void* DefaultAllocator::ReallocateAligned(void* ptr, size_t newSize, size_t align)
	{
		return IsSmallAlloc(*this, ptr) ? ReallocSmallAligned(*this, ptr, newBytes, align) : _aligned_realloc(ptr, newBytes, align);
	}

	void DefaultAllocator::FreeAligned(void* ptr)
	{
		if (IsSmallAlloc(*this, ptr))
			FreeSmall(*this, ptr);
		else
			_aligned_free(ptr);
	}
#endif

	size_t DefaultAllocator::GetMaxAllocationSize()
	{
		return std::numeric_limits<size_t>::max();
	}

	LinearAllocator::LinearAllocator()
	{
	}

	LinearAllocator::~LinearAllocator()
	{
	}

	void* LinearAllocator::Allocate(size_t size, const char* filename, int line)
	{
		return nullptr;
	}

	void* LinearAllocator::Allocate(size_t size)
	{
		return nullptr;
	}

	void* LinearAllocator::Reallocate(void* ptr, size_t newBytes, const char* filename, int line)
	{
		return nullptr;
	}

	void LinearAllocator::Free(void* ptr)
	{
	}

	void* LinearAllocator::AllocateAligned(size_t size, size_t align, const char* filename, int line)
	{
		return nullptr;
	}

	void* LinearAllocator::ReallocateAligned(void* ptr, size_t newBytes, size_t align, const char* filename, int line)
	{
		return nullptr;
	}

	void LinearAllocator::FreeAligned(void* ptr)
	{
	}
}