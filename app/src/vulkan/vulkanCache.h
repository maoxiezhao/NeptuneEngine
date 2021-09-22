#pragma once

#include "utils\hash.h"
#include "utils\objectPool.h"

#include <unordered_map>

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
		for (auto& kvp : mHashMap)
		{
			mPool.free(kvp.second);
		}
		mHashMap.clear();	
	}

	HashMapIterator find(HashValue hash)
	{
		return mHashMap.find(hash);
	}

	HashMapConstIterator find(HashValue hash)const
	{
		return mHashMap.find(hash);
	}

	T& operator[](HashValue hash)
	{
		auto it = find(hash);
		if (it != mHashMap.end())
		{
			return *it->second;
		}
			
		return *(emplace(hash));
	}

	void erase(T* value)
	{
		for (auto& kvp : mHashMap)
		{
			if (kvp.second == value)
			{
				mHashMap.erase(kvp.first);
				mPool.free(value);
				break;
			}
		}
	}

	void erase(HashValue hash)
	{
		auto it = find(hash);
		if (it != mHashMap.end())
		{
			mPool.free(it->second);
			mHashMap.erase(it);
		}
	}

	void free(T* value)
	{
		mPool.free(value);
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
		return mPool.allocate(std::forward<Args>(args)...);
	}

	T* insert(HashValue hash, T* value)
	{
		auto it = mHashMap.find(hash);
		if (it != mHashMap.end())
		{
			mPool.free(it->second);
		}
		mHashMap.insert(std::make_pair(hash, value));
		return value;
	}

	HashMapIterator begin()
	{
		return mHashMap.begin();
	}

	HashMapIterator end()
	{
		return mHashMap.end();
	}

private:
	HashMap mHashMap;
	Util::ObjectPool<T> mPool;
};