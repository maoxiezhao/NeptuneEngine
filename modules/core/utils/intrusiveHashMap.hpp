#pragma once

#include "objectPool.h"
#include "math\hash.h"

#include <unordered_map>

namespace VulkanTest
{
namespace Util
{
	template <typename T>
	class IntrusiveHashMap
	{
	public:
		~IntrusiveHashMap()
		{
			Clear();
		}

		IntrusiveHashMap() = default;
		IntrusiveHashMap(const IntrusiveHashMap&) = delete;
		void operator=(const IntrusiveHashMap&) = delete;

		void Clear()
		{
			for (auto& kvp : hashmap)
				pool.free(kvp.second);

			hashmap.clear();
		}

		T* Find(HashValue hash) const
		{
			auto it = hashmap.find(hash);
			if (it == hashmap.end())
				return nullptr;

			return it->second;
		}

		T& operator[](HashValue hash)
		{
			auto* t = Find(hash);
			if (!t)
				t = EmplaceYield(hash);
			return *t;
		}

		void Erase(T* value)
		{
			for (auto& kvp : hashmap)
			{
				if (kvp.second == value)
				{
					hashmap.erase(kvp.first);
					pool.free(value);
					break;
				}
			}
		}

		void Erase(HashValue hash)
		{
			auto it = hashmap.find(hash);
			if (it != hashmap.end())
			{
				pool.free(it->second);
				hashmap.erase(it);
			}
		}

		template <typename... Args>
		T* EmplaceYield(HashValue hash, Args&&... args)
		{
			T* t = Allocate(std::forward<Args>(args)...);
			return InsertYield(hash, t);
		}

		template <typename... Args>
		T* Allocate(Args&&... args)
		{
			return pool.allocate(std::forward<Args>(args)...);
		}

		void Free(T* value)
		{
			pool.free(value);
		}

		T* InsertYield(HashValue hash, T* value)
		{
			auto it = hashmap.find(hash);
			if (it != hashmap.end())
				pool.free(it->second);	

			hashmap[hash] = value;
			return value;
		}

		typename std::unordered_map<HashValue, T*>::const_iterator begin() const
		{
			return hashmap.begin();
		}

		typename std::unordered_map<HashValue, T*>::const_iterator end() const
		{
			return hashmap.end();
		}

	private:
		std::unordered_map<HashValue, T*> hashmap;
		ObjectPool<T> pool;
	};

}
}