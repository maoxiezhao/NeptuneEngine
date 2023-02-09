#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <stdlib.h>

#include "core\platform\sync.h"

namespace VulkanTest
{
namespace Util
{
	template<typename T>
	class ObjectPool
	{
	public:
		template<typename... Args>
		T* allocate(Args &&... args)
		{
#ifndef OBJECT_POOL_DEBUG
			if (vacants.empty())
			{
				unsigned num_objects = 64u << memory.size();
				T* ptr = static_cast<T*>(_aligned_malloc(num_objects * sizeof(T), std::max(size_t(64), alignof(T))));
				if (!ptr)
					return nullptr;

				for (unsigned i = 0; i < num_objects; i++)
					vacants.push_back(&ptr[i]);

				memory.emplace_back(ptr);
			}

			T* ptr = vacants.back();
			vacants.pop_back();
			new(ptr) T(std::forward<Args>(args)...);
			return ptr;
#else
			return new T(std::forward<Args>(args)...);
#endif
		}

		void free(T* ptr)
		{
#ifndef OBJECT_POOL_DEBUG
			ptr->~T();
			vacants.push_back(ptr);
#else
			delete ptr;
#endif
		}

		void clear()
		{
#ifndef OBJECT_POOL_DEBUG
			vacants.clear();
			memory.clear();
#endif
		}

	protected:
#ifndef OBJECT_POOL_DEBUG
		std::vector<T*> vacants;

		struct MallocDeleter
		{
			void operator()(T* ptr)
			{
				_aligned_free(ptr);
			}
		};

		std::vector<std::unique_ptr<T, MallocDeleter>> memory;
#endif
	};

	template<typename T>
	class ThreadSafeObjectPool : private ObjectPool<T>
	{
	public:
		template<typename... Args>
		T* allocate(Args&&... args)
		{
			ScopedMutex lock(mutex);
			return ObjectPool<T>::allocate(std::forward<Args>(args)...);
		}

		void free(T* ptr)
		{
			ptr->~T();
			ScopedMutex lock(mutex);
			this->vacants.push_back(ptr);
		}

		void clear()
		{
			ScopedMutex lock(mutex);
			ObjectPool<T>::clear();
		}

	private:
		 Mutex mutex;
	};
}
}