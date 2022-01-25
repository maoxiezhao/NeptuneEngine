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

	template<typename T>
	class IntrusivePODWrapper : public HashedObject<IntrusivePODWrapper<T>>
	{
	public:
		template<typename U>
		explicit IntrusivePODWrapper(U&& obj_) : obj(std::forward<U>(obj_)) {}
		IntrusivePODWrapper() = default;

		T& Get() { return obj; }
		const T& Get()const { return obj; }

		T obj = {};
	};

	// IntrusiveHashMapHolder just manage a list of T pointers
	// It's only used for ThreadSafeIntrusiveHashMapReadCached
	template <typename T>
	class IntrusiveHashMapHolder
	{
	public:
		static const U32 INITIAL_CAPACITY = 16;
		static const U32 INITIAL_DEPTH = 3;

		~IntrusiveHashMapHolder()
		{
			clear();
		}

		void clear()
		{
			list.clear();
			hashTable.clear();
			depth = 0;
		}

		void clearAndKeepCapacity()
		{
			for (int i = 0; i < hashTable.size(); i++)
				hashTable[i] = nullptr;
			list.clear();
		}

		T* find(HashValue hash)const
		{
			if (hashTable.empty())
				return nullptr;

			HashValue marked = hash & hashMask;
			for (U32 i = 0; i < depth; i++)
			{
				if (hashTable[marked] != nullptr && GetItemHash(hashTable[marked]) == hash)
					return hashTable[marked];
				marked = (hash + 1) & hashMask;
			}
			return nullptr;
		}

		T* insertYield(T* value)
		{
			if (hashTable.empty())
				Grow();

			HashValue hash = GetItemHash(value);
			HashValue marked = hash & hashMask;
			for (U32 i = 0; i < depth; i++)
			{
				if (hashTable[marked] != nullptr && GetItemHash(hashTable[marked]) == hash)
				{
					// Insert stop while value already exsist
					return hashTable[marked];
				}

				if (hashTable[marked] == nullptr)
				{
					hashTable[marked] = value;
					list.push_front(value);
					return nullptr;
				}

				if (i > 1)
				{
					U32 probeDist = ProbeDistanceHash(GetItemHash(hashTable[marked]), marked);
					if (probeDist < (i - 1))
					{
						T* ret = hashTable[marked];
						hashTable[marked] = value;
						auto it = std::find(list.begin(), list.end(), ret);
						if (it != list.end())
							*it = value;

						i = probeDist;
						value = ret;
						hash = GetItemHash(value);
					}
				}
				marked = (marked + 1) & hashMask;
			}

			Grow();
			return insertYield(value);
		}

		T* insertReplace(T* value)
		{
			if (hashTable.empty())
				Grow();

			HashValue hash = GetItemHash(value);
			HashValue marked = hash & hashMask;
			for (U32 i = 0; i < depth; i++)
			{
				if (hashTable[marked] != nullptr && GetItemHash(hashTable[marked]) == hash)
				{
					// New value will replace old value, then return old value
					T* ret = hashTable[marked];
					hashTable[marked] = value;
					auto it = std::find(list.begin(), list.end(), ret);
					if (it != list.end())
						list.erase(it);
					list.push_front(value);
					return ret;
				}

				if (hashTable[marked] == nullptr)
				{
					hashTable[marked] = value;
					list.push_front(value);
					return nullptr;
				}

				if (i > 1)
				{
					U32 probeDist = ProbeDistanceHash(GetItemHash(hashTable[marked]), marked);
					if (probeDist < (i - 1))
					{
						T* ret = hashTable[marked];
						hashTable[marked] = value;
						auto it = std::find(list.begin(), list.end(), ret);
						if (it != list.end())
							*it = value;

						i = probeDist;
						value = ret;
						hash = GetItemHash(value);
					}
				}
				marked = (marked + 1) & hashMask;
			}

			Grow();
			return insertYield(value);
		}

		T* erase(HashValue hash)
		{
			if (hashTable.empty())
				return nullptr;

			HashValue marked = hash & hashMask;
			for (U32 i = 0; i < depth; i++)
			{
				if (hashTable[marked] != nullptr && GetItemHash(hashTable[marked]) == hash)
				{
					T* ret = hashTable[marked];
					hashTable[marked] = nullptr;
					auto it = std::find(list.begin(), list.end(), ret);
					if (it != list.end())
						list.erase(it);
					return ret;
				}
				
				marked = (marked + 1) & hashMask;
			}
			return nullptr;
		}

		T* erase(T* value)
		{
			return erase(GetItemHash(value));
		}

		const std::list<T*>& GetList()const
		{
			return list;
		}

		std::list<T*>& GetList()
		{
			return list;
		}

		typename std::list<T*>::const_iterator begin() const
		{
			return list.begin();
		}
		typename std::list<T*>::const_iterator end() const
		{
			return list.end();
		}
		typename std::list<T*>::iterator begin()
		{
			return list.begin();
		}
		typename std::list<T*> ::iterator end()
		{
			return list.end();
		}

	private:
		U32 ProbeDistanceHash(HashValue hash, HashValue curPos)
		{
			return (curPos - (hash & hashMask) + hashTable.size()) & hashMask;
		}

		inline HashValue GetItemHash(const T* value) const
		{
			return value != nullptr ? static_cast<const HashedObject<T> *>(value)->GetHash() : ~0u;
		}

		void Grow()
		{
			auto InsertHashTable = [&](T* value)->bool {

				HashValue hash = GetItemHash(value);
				HashValue hashMask = hashTable.size() - 1;
				HashValue marked = hash & hashMask;
				for (U32 i = 0; i < depth; i++)
				{
					if (hashTable[marked] == nullptr)
					{
						hashTable[marked] = value;
						return true;
					}
					marked = (hash + 1) & hashMask;
				}
				return false;
			};
		
			bool success = false;
			while (success == false)
			{
				for (int i = 0; i < hashTable.size(); i++)
					hashTable[i] = nullptr;

				if (hashTable.empty())
				{
					hashTable.resize(INITIAL_CAPACITY);
					depth = INITIAL_DEPTH;
				}
				else
				{
					hashTable.resize(hashTable.size() * 2);
					depth++;
				}

				hashMask = hashTable.size() - 1;
				success = true;

				for (auto item : list)
				{
					if (!InsertHashTable(item))
					{
						success = false;
						break;
					}
				}
			}
		}

		std::vector<T*> hashTable;
		std::list<T*> list;
		U32 depth = 0;
		U32 hashMask = 0;
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
			auto& list = readWrite.GetList();
			for (auto value : list)
			{
				T* old = readOnly.insertYield(value);
				if (old != nullptr)
					pool.free(old);
			}
			readWrite.clearAndKeepCapacity();
		}

		void clear()
		{
			auto ClearInnerList = [&](std::list<T*>& list) {
				for (auto value : list)
				{
					if (value != nullptr)
						pool.free(value);
				}
				list.clear();
			};
			ScopedWriteLock holder(lock);
			ClearInnerList(readOnly.GetList());
			ClearInnerList(readWrite.GetList());
			readOnly.clear();
			readWrite.clear();
		}

		T* find(HashValue hash) const
		{
			T* ret = readOnly.find(hash);
			if (ret != nullptr)
				return ret;

			ScopedReadLock holder(lock);
			return readWrite.find(hash);
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
			if (it != nullptr)
			{
				pool.free(it);
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
			SetItemHash(value, hash);
			T* old = readWrite.insertYield(value);
			if (old != nullptr)
				pool.free(old);
			return value;
		}
		
		IntrusiveHashMapHolder<T>& GetReadOnly()
		{
			return readOnly;
		}

		IntrusiveHashMapHolder<T>& GetReadWrite()
		{
			return readWrite;
		}

	private:
		void SetItemHash(T* value, HashValue hash)
		{
			static_cast<HashedObject<T>*>(value)->SetHash(hash);
		}

		IntrusiveHashMapHolder<T> readOnly;
		IntrusiveHashMapHolder<T> readWrite;
		
		ObjectPool<T> pool;
		mutable RWLock lock;
	};
}
}