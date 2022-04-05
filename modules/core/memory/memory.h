#pragma once

#include "allocators.h"
#include "memTracker.h"

namespace VulkanTest
{
	struct NewPlaceHolder {};

#if (VULKAN_MEMORY_ALLOCATOR == VULKAN_MEMORY_ALLOCATOR_DEFAULT)
	using MemoryAllocator = DefaultAllocator;
#endif

#if (VULKAN_CONTAINER_ALLOCATOR == VULKAN_MEMORY_ALLOCATOR_DEFAULT)
	using ContainerAllocator = DefaultAllocator;
#endif

	class Memory
	{
	public:
#ifdef VULKAN_MEMORY_TRACKER
		static void* Alloc(size_t size, const char* filename, int line);
		static void* Realloc(void* ptr, size_t newBytes, const char* filename, int line);
		static void  Free(void* ptr);
		static void* AllocAligned(size_t size, size_t align, const char* filename, int line);
		static void* ReallocAligned(void* ptr, size_t newBytes, size_t align, const char* filename, int line);
		static void  FreeAligned(void* ptr);
#else
		static void* Alloc(size_t size);
		static void* Realloc(void* ptr, size_t newBytes);
		static void  Free(void* ptr);
		static void* AllocAligned(size_t size, size_t align);
		static void* ReallocAligned(void* ptr, size_t newBytes, size_t align);
		static void  FreeAligned(void* ptr);
#endif
		static void  Memmove(void* dst, const void* src, size_t size);
		static void  Memcpy(void* dst, const void* src, size_t size);
		static void  Memset(void* dst, int c, int count);

		template<typename T>
		static void ObjectConstruct(T* ptr)
		{
			if (ptr == nullptr) {
				return;
			}
			if (!__has_trivial_destructor(T)) {
				ptr->~T();
			}
		}

		template<typename T>
		static T* ArrayConstructFunc(T* mem, size_t num)
		{
			T* elems = (T*)mem;
			if (!__has_trivial_constructor(T)) {
				for (size_t i = 0; i < num; i++) {
					new(&elems[i]) T();
				}
			}
			return elems;
		}

		template<typename T>
		static void ArrayDeconstructFunc(T* ptrArr, size_t num)
		{
			uint64_t* mem = (uint64_t*)ptrArr;
			if (mem == nullptr) {
				return;
			}
			if (!__has_trivial_destructor(T)) 
			{
				for (size_t i = 0; i < num; i++) {
					ptrArr[i].~T();
				}
			}
		}
	};
}

inline void* operator new(size_t size, VulkanTest::NewPlaceHolder, void* ptr) { return ptr; }
inline void operator delete(void*, VulkanTest::NewPlaceHolder, void*) { }

#ifdef  VULKAN_MEMORY_TRACKER
#define CJING_NEW(T) new (VulkanTest::NewPlaceHolder(), VulkanTest::Memory::Alloc(sizeof(T), __FILE__, __LINE__)) T
#define CJING_DELETE(ptr) VulkanTest::Memory::ObjectConstruct(ptr); VulkanTest::Memory::Free(ptr);
#define CJING_NEW_ARR(T, count) VulkanTest::Memory::ArrayConstructFunc(static_cast<T*>(VulkanTest::Memory::Alloc(sizeof(T) * count, __FILE__, __LINE__)), count)
#define CJING_DELETE_ARR(ptr, count) VulkanTest::Memory::ArrayDeconstructFunc(ptr, count); VulkanTest::Memory::Free(ptr);

#define CJING_MALLOC(size)  VulkanTest::Memory::Alloc(size, __FILE__, __LINE__)
#define CJING_MALLOC_ALIGN(size, align)  VulkanTest::Memory::AlignAlloc(size, align, __FILE__, __LINE__)
#define CJING_REMALLOC(ptr, size)  VulkanTest::Memory::Realloc(ptr, size, __FILE__, __LINE__)
#define CJING_REMALLOC_ALIGN(ptr, size, align)  VulkanTest::Memory::AlignRealloc(ptr, size, align, __FILE__, __LINE__)
#define CJING_FREE(ptr) VulkanTest::Memory::Free(ptr);
#define CJING_FREE_ALIGN(ptr) VulkanTest::Memory::AlignFree(ptr);

#define CJING_ALLOCATOR_MALLOC(allocator, size)  allocator.Allocate(size, __FILE__, __LINE__)
#define CJING_ALLOCATOR_MALLOC_ALIGN(allocator, size, align)  allocator.AlignAllocate(size, align, __FILE__, __LINE__)
#define CJING_ALLOCATOR_REMALLOC(allocator, ptr, size)  allocator.Reallocate(ptr, size, __FILE__, __LINE__)
#define CJING_ALLOCATOR_REMALLOC_ALIGN(allocator, ptr, size, align)  allocator.AlignReallocate(ptr, size, align, __FILE__, __LINE__)
#define CJING_ALLOCATOR_FREE(allocator, ptr) allocator.Free(ptr);
#define CJING_ALLOCATOR_FREE_ALIGN(allocator, ptr) allocator.AlignFree(ptr); 
#define CJING_ALLOCATOR_NEW(allocator, T) new (VulkanTest::NewPlaceHolder(), allocator.Allocate(sizeof(T), __FILE__, __LINE__)) T
#define CJING_ALLOCATOR_DELETE(allocator, ptr) VulkanTest::Memory::ObjectConstruct(ptr); allocator.Free(ptr);

#else
#define CJING_NEW(T) new (VulkanTest::NewPlaceHolder(), VulkanTest::Memory::Alloc(sizeof(T))) T
#define CJING_DELETE(ptr) VulkanTest::Memory::ObjectConstruct(ptr); VulkanTest::Memory::Free(ptr);
#define CJING_NEW_ARR(T, count) VulkanTest::Memory::ArrayConstructFunc(static_cast<T*>(VulkanTest::Memory::Alloc(sizeof(T) * count)), count)
#define CJING_DELETE_ARR(ptr, count) VulkanTest::Memory::ArrayDeconstructFunc(ptr, count); VulkanTest::Memory::Free(ptr);

#define CJING_MALLOC(size)  VulkanTest::Memory::Alloc(size)
#define CJING_MALLOC_ALIGN(size, align)  VulkanTest::Memory::AlignAlloc(size, align)
#define CJING_REMALLOC(ptr, size)  VulkanTest::Memory::Realloc(ptr, size)
#define CJING_REMALLOC_ALIGN(ptr, size, align)  VulkanTest::Memory::AlignRealloc(ptr, size, align)
#define CJING_FREE(ptr) VulkanTest::Memory::Free(ptr);
#define CJING_FREE_ALIGN(ptr) VulkanTest::Memory::AlignFree(ptr); 

#define CJING_ALLOCATOR_MALLOC(allocator, size)  allocator.Allocate(size)
#define CJING_ALLOCATOR_MALLOC_ALIGN(allocator, size, align)  allocator.AlignAllocate(size, align)
#define CJING_ALLOCATOR_REMALLOC(allocator, ptr, size)  allocator.Reallocate(ptr, size)
#define CJING_ALLOCATOR_REMALLOC_ALIGN(allocator, ptr, size, align)  allocator.AlignReallocate(ptr, size, align)
#define CJING_ALLOCATOR_FREE(allocator, ptr) allocator.Free(ptr);
#define CJING_ALLOCATOR_FREE_ALIGN(allocator, ptr) allocator.AlignFree(ptr); 
#define CJING_ALLOCATOR_NEW(allocator, T) new (VulkanTest::NewPlaceHolder(), allocator.Allocate(size)) T
#define CJING_ALLOCATOR_DELETE(allocator, ptr) VulkanTest::Memory::ObjectConstruct(ptr); allocator.Free(ptr);


#endif

#define CJING_SAFE_DELETE(ptr) { if (ptr) { CJING_DELETE(ptr); ptr = nullptr; } }
#define CJING_SAFE_DELETE_ARR(ptr, count) { if (ptr) { CJING_DELETE_ARR(ptr, count); ptr = nullptr; } }
#define CJING_SAFE_FREE(ptr) { if (ptr) { CJING_FREE(ptr); ptr = nullptr; } }

// smart pointer
namespace VulkanTest
{
#ifdef USE_STL_SMART_POINTER
	
	///////////////////////////////////////////////////////////////////////
	// Shared pointer
	template<typename T>
	inline void SmartPointDeleter(T* v)
	{
		CJING_DELETE(v);
	}

	template<typename T>
	using SharedPtr = std::shared_ptr<T>;

	template< typename T>
	SharedPtr<T> CJING_SHARED(T* ptr)
	{
		return SharedPtr<T>(ptr, SmartPointDeleter<T>);
	}

	template< typename T, typename... Args >
	SharedPtr<T> CJING_MAKE_SHARED(Args&&... args)
	{
		return SharedPtr<T>(
			CJING_NEW(T){ std::forward<Args>(args)... },
			SmartPointDeleter<T>
		);
	}

	template<typename T>
	using ENABLE_SHARED_FROM_THIS = std::enable_shared_from_this<T>;

#endif

	///////////////////////////////////////////////////////////////////////
	// Unique pointer
	template<typename T>
	class UniquePtr
	{
	public:
		UniquePtr() : mPtr(nullptr) {}
		UniquePtr(T* ptr) : mPtr(ptr) {}
		~UniquePtr()
		{
			if (mPtr) {
				CJING_DELETE(mPtr);
			}
		}

		template <typename T2>
		UniquePtr(UniquePtr<T2>&& rhs)
		{
			*this = static_cast<UniquePtr<T2>&&>(rhs);
		}

		template <typename T2>
		UniquePtr& operator=(UniquePtr<T2>&& rhs)
		{
			if (mPtr) {
				CJING_DELETE(mPtr);
			}

			mPtr = static_cast<T*>(rhs.Detach());
			return *this;
		}

		UniquePtr(const UniquePtr& rhs) = delete;
		UniquePtr& operator=(const UniquePtr& rhs) = delete;

		UniquePtr&& Move() { return static_cast<UniquePtr&&>(*this); }

		T* Detach()
		{
			T* ret = mPtr;
			mPtr = nullptr;
			return ret;
		}

		void Reset()
		{
			if (mPtr) {
				CJING_DELETE(mPtr);
			}
			mPtr = nullptr;
		}

		T* Get()const {
			return mPtr;
		}
		T& operator*() const {
			return *mPtr;
		}
		T* operator->()const {
			return mPtr;
		}
		explicit operator bool()const {
			return mPtr != nullptr;
		}

	private:
		T* mPtr = nullptr;
	};

	template< typename T>
	UniquePtr<T> CJING_UNIQUE(T* ptr)
	{
		return UniquePtr<T>(ptr);
	}

	template< typename T, typename... Args >
	UniquePtr<T> CJING_MAKE_UNIQUE(Args&&... args)
	{
		return UniquePtr<T>(
			CJING_NEW(T) { static_cast<Args&&>(args)... }
		);
	}

	///////////////////////////////////////////////////////////////////////
	// Local pointer
	template<typename T>
	class LocalPtr
	{
	public:
		void operator =(const LocalPtr&) = delete;

		~LocalPtr()
		{
			if (obj) obj->~T();
		}

		template<typename... Args>
		void Create(Args&&... args)
		{
			obj = new (NewPlaceHolder(), mem) T(std::forward<Args>(args)...);
		}

		void Destroy()
		{
			ASSERT(obj);
			obj->~T();
			obj = nullptr;
		}

		T& operator*() { ASSERT(obj); return *obj; }
		T* operator->() const { ASSERT(obj); return obj; }
		T* Get() const { return obj; }

	private:
		alignas(T) U8 mem[sizeof(T)];
		T* obj = nullptr;
	};
}