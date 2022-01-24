#pragma once

#include "core\platform\sync.h"
#include "objectPool.h"
#include "math\hash.h"

#include <unordered_map>
#include <list>

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
			clear();
		}

		IntrusiveHashMap() = default;
		IntrusiveHashMap(const IntrusiveHashMap&) = delete;
		void operator=(const IntrusiveHashMap&) = delete;

		void clear()
		{
			for (auto& kvp : hashmap)
				pool.free(kvp.second);

			hashmap.clear();
		}

		T* find(HashValue hash) const
		{
			auto it = hashmap.find(hash);
			if (it == hashmap.end())
				return nullptr;

			return it->second;
		}

		T& operator[](HashValue hash)
		{
			auto* t = find(hash);
			if (!t)
				t = emplace(hash);
			return *t;
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
			auto it = hashmap.find(hash);
			if (it != hashmap.end())
			{
				pool.free(it->second);
				hashmap.erase(it);
			}
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

		void free(T* value)
		{
			pool.free(value);
		}

		T* insert(HashValue hash, T* value)
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

	template <typename T>
	class ThreadSafeIntrusiveHashMap
	{
	public:
		~ThreadSafeIntrusiveHashMap()
		{
			clear();
		}

		ThreadSafeIntrusiveHashMap() = default;
		ThreadSafeIntrusiveHashMap(const ThreadSafeIntrusiveHashMap&) = delete;
		void operator=(const ThreadSafeIntrusiveHashMap&) = delete;

		void clear()
		{
			ScopedWriteLock holder(lock);
			for (auto& kvp : hashmap)
				pool.free(kvp.second);
			hashmap.clear();
		}

		T* find(HashValue hash) const
		{
			ScopedReadLock holder(lock);
			auto it = hashmap.find(hash);
			if (it == hashmap.end())
				return nullptr;

			return it->second;
		}

		T& operator[](HashValue hash)
		{
			auto* t = find(hash);
			if (!t)
				t = emplace(hash);
			return *t;
		}

		void erase(T* value)
		{
			ScopedWriteLock holder(lock);
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
			ScopedWriteLock holder(lock);
			auto it = hashmap.find(hash);
			if (it != hashmap.end())
			{
				pool.free(it->second);
				hashmap.erase(it);
			}
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
			ScopedWriteLock holder(lock);
			return pool.allocate(std::forward<Args>(args)...);
		}

		void free(T* value)
		{
			ScopedWriteLock holder(lock);
			pool.free(value);
		}

		T* insert(HashValue hash, T* value)
		{
			ScopedWriteLock holder(lock);
			auto it = hashmap.find(hash);
			if (it != hashmap.end())
				pool.free(it->second);

			hashmap[hash] = value;
			return value;
		}

	private:
		std::unordered_map<HashValue, T*> hashmap;
		ObjectPool<T> pool;
		mutable RWLock lock;
	};


	template <typename T>
	class ThreadSafeIntrusiveHashMapReadCached
	{
	public:
		~ThreadSafeIntrusiveHashMapReadCached()
		{
			clear();
		}

		ThreadSafeIntrusiveHashMapReadCached() = default;
		ThreadSafeIntrusiveHashMapReadCached(const ThreadSafeIntrusiveHashMapReadCached&) = delete;
		void operator=(const ThreadSafeIntrusiveHashMapReadCached&) = delete;

		void MoveToReadOnly()
		{
			while (!readWrite.empty())
			{
				auto kvp = readWrite.begin();
				auto it = readWrite.find(kvp->first);
				if (it != readWrite.end())
					pool.free(it->second);
				readWrite[kvp->first] = kvp->second;
				readWrite.erase(kvp);
			}
		}

		void clear()
		{
			ScopedWriteLock holder(lock);
			for (auto& kvp : readOnly)
				pool.free(kvp.second);
			for (auto& kvp : readWrite)
				pool.free(kvp.second);
			readOnly.clear();
			readWrite.clear();
		}

		T* find(HashValue hash) const
		{
			auto it = readOnly.find(hash);
			if (it != readOnly.end())
				return it->second;

			ScopedReadLock holder(lock);
			it = readWrite.find(hash);
			if (it == readWrite.end())
				return nullptr;
			return it->second;
		}

		T& operator[](HashValue hash)
		{
			auto* t = find(hash);
			if (!t)
				t = emplace(hash);
			return *t;
		}

		void erase(T* value)
		{
			ScopedWriteLock holder(lock);
			for (auto& kvp : readWrite)
			{
				if (kvp.second == value)
				{
					readWrite.erase(kvp.first);
					pool.free(value);
					break;
				}
			}
		}

		void erase(HashValue hash)
		{
			ScopedWriteLock holder(lock);
			auto it = readWrite.find(hash);
			if (it != readWrite.end())
			{
				pool.free(it->second);
				readWrite.erase(it);
			}
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
			ScopedWriteLock holder(lock);
			return pool.allocate(std::forward<Args>(args)...);
		}

		void free(T* value)
		{
			ScopedWriteLock holder(lock);
			pool.free(value);
		}

		T* insert(HashValue hash, T* value)
		{
			ScopedWriteLock holder(lock);
			auto it = readWrite.find(hash);
			if (it != readWrite.end())
				pool.free(it->second);

			readWrite[hash] = value;
			return value;
		}
		
		std::unordered_map<HashValue, T*>& GetReadOnly()
		{
			return readOnly;
		}

		std::unordered_map<HashValue, T*>& GetReadWrite()
		{
			return readWrite;
		}

	private:
		std::unordered_map<HashValue, T*> readOnly;
		std::unordered_map<HashValue, T*> readWrite;
		ObjectPool<T> pool;
		mutable RWLock lock;
	};
}
}