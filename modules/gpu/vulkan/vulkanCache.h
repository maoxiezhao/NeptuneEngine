#pragma once

#include "core\utils\hash.h"
#include "core\utils\objectPool.h"

#include <unordered_map>

namespace GPU
{
	template<typename T>
	class VulkanCache
	{
	public:
		using HashMap = typename std::unordered_map<HashValue, T*>;
		using HashMapIterator = typename HashMap::iterator;
		using HashMapConstIterator = typename HashMap::const_iterator;

		~VulkanCache()
		{
			Clear();
		}

		void Clear()
		{
			for (auto& kvp : hashmap)
			{
				pool.free(kvp.second);
			}
			hashmap.clear();
		}

		HashMapIterator find(HashValue hash)
		{
			return hashmap.find(hash);
		}

		HashMapConstIterator find(HashValue hash)const
		{
			return hashmap.find(hash);
		}

		T& operator[](HashValue hash)
		{
			auto it = find(hash);
			if (it != hashmap.end())
			{
				return *it->second;
			}

			return *(emplace(hash));
		}

		void erase(T* value)
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

		void erase(HashValue hash)
		{
			auto it = find(hash);
			if (it != hashmap.end())
			{
				pool.free(it->second);
				hashmap.erase(it);
			}
		}

		void free(T* value)
		{
			pool.free(value);
		}

		template <typename... Args>
		T* emplace(HashValue hash, Args&&... args)
		{
			T* t = allocate(std::forward<Args>(args)...);
			return insert(hash, t);
		}

		template <typename... Args>
		T* allocate(Args&&... args)
		{
			return pool.allocate(std::forward<Args>(args)...);
		}

		T* insert(HashValue hash, T* value)
		{
			auto it = hashmap.find(hash);
			if (it != hashmap.end())
			{
				pool.free(it->second);
			}
			hashmap.insert(std::make_pair(hash, value));
			return value;
		}

		HashMapIterator begin()
		{
			return hashmap.begin();
		}

		HashMapIterator end()
		{
			return hashmap.end();
		}

	private:
		HashMap hashmap;
		Util::ObjectPool<T> pool;
	};

}