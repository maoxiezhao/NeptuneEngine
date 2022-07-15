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
	struct IntrusiveListNode
	{
		IntrusiveListNode<T>* prev = nullptr;
		IntrusiveListNode<T>* next = nullptr;
	};

	template <typename T>
	class IntrusiveList
	{
	public:
		void clear()
		{
			head = nullptr;
			tail = nullptr;
		}

		class Iterator
		{
		public:
			friend class IntrusiveList<T>;
			Iterator(IntrusiveListNode<T>* node_)
				: node(node_)
			{
			}

			Iterator() = default;

			explicit operator bool() const
			{
				return node != nullptr;
			}

			bool operator==(const Iterator& other) const
			{
				return node == other.node;
			}

			bool operator!=(const Iterator& other) const
			{
				return node != other.node;
			}

			T& operator*()
			{
				return *static_cast<T*>(node);
			}

			const T& operator*() const
			{
				return *static_cast<T*>(node);
			}

			T* get()
			{
				return static_cast<T*>(node);
			}

			const T* get() const
			{
				return static_cast<const T*>(node);
			}

			T* operator->()
			{
				return static_cast<T*>(node);
			}

			const T* operator->() const
			{
				return static_cast<T*>(node);
			}

			Iterator& operator++()
			{
				node = node->next;
				return *this;
			}

			Iterator& operator--()
			{
				node = node->prev;
				return *this;
			}

		private:
			IntrusiveListNode<T>* node = nullptr;
		};

		Iterator begin() const
		{
			return Iterator(head);
		}

		Iterator rbegin() const
		{
			return Iterator(tail);
		}

		Iterator end() const
		{
			return Iterator();
		}

		Iterator erase(Iterator itr)
		{
			auto* node = itr.get();
			auto* next = node->next;
			auto* prev = node->prev;

			if (prev)
				prev->next = next;
			else
				head = next;

			if (next)
				next->prev = prev;
			else
				tail = prev;

			return next;
		}

		void insert_front(Iterator itr)
		{
			auto* node = itr.get();
			if (head)
				head->prev = node;
			else
				tail = node;

			node->next = head;
			node->prev = nullptr;
			head = node;
		}

		void insert_back(Iterator itr)
		{
			auto* node = itr.get();
			if (tail)
				tail->next = node;
			else
				head = node;

			node->prev = tail;
			node->next = nullptr;
			tail = node;
		}

		bool empty() const
		{
			return head == nullptr;
		}

	private:
		IntrusiveListNode<T>* head = nullptr;
		IntrusiveListNode<T>* tail = nullptr;
	};

	template <typename T>
	class IntrusiveHashMapEnabled : public IntrusiveListNode<T>
	{
	public:
		IntrusiveHashMapEnabled() = default;
		IntrusiveHashMapEnabled(HashValue hash)
			: hashValue(hash)
		{
		}

		void SetHash(HashValue hash)
		{
			hashValue = hash;
		}

		HashValue GetHash() const
		{
			return hashValue;
		}

	private:
		HashValue hashValue = 0;
	};

	template<typename T>
	class IntrusivePODWrapper : public IntrusiveHashMapEnabled<IntrusivePODWrapper<T>>
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

		T* find(HashValue hash)const
		{
			if (hashTable.empty())
				return nullptr;

			HashValue hashMask = GetHashMask();
			HashValue masked = hash & hashMask;
			for (U32 i = 0; i < depth; i++)
			{
				if (hashTable[masked] != nullptr && GetItemHash(hashTable[masked]) == hash)
					return hashTable[masked];
				masked = (hash + 1) & hashMask;
			}
			return nullptr;
		}

		T* insert_yield(T*& value)
		{
			if (hashTable.empty())
				Grow();

			HashValue hashMask = GetHashMask();
			HashValue hash = GetItemHash(value);
			HashValue masked = hash & hashMask;
			for (U32 i = 0; i < depth; i++)
			{
				if (hashTable[masked] != nullptr && GetItemHash(hashTable[masked]) == hash)
				{
					T* ret = value;
					value = hashTable[masked];
					return ret;
				}
				else if (hashTable[masked] == nullptr)
				{
					hashTable[masked] = value;
					list.insert_front(value);
					return nullptr;
				}
				masked = (masked + 1) & hashMask;
			}

			Grow();
			return insert_yield(value);
		}

		T* insert_replace(T* value)
		{
			if (hashTable.empty())
				Grow();

			HashValue hashMask = GetHashMask();
			HashValue hash = GetItemHash(value);
			HashValue masked = hash & hashMask;
			for (U32 i = 0; i < depth; i++)
			{
				if (hashTable[masked] != nullptr && GetItemHash(hashTable[masked]) == hash)
				{
					// New value will replace old value, then return old value
					T* ret = hashTable[masked];
					hashTable[masked] = value;
					list.erase(ret);
					list.insert_front(value);
					return ret;
				}

				if (hashTable[masked] == nullptr)
				{
					assert(!hashTable[masked]);
					hashTable[masked] = value;
					list.insert_front(value);
					return nullptr;
				}
#if 0
				if (i > 1)
				{
					U32 probeDist = ProbeDistanceHash(GetItemHash(hashTable[masked]), masked);
					if (probeDist < (i - 1))
					{
						T* ret = hashTable[masked];
						hashTable[masked] = value;
						list.erase(ret);

						i = probeDist;
						value = ret;
						hash = GetItemHash(value);
					}
				}
#endif
				masked = (masked + 1) & hashMask;
			}

			Grow();
			return insert_replace(value);
		}

		T* erase(HashValue hash)
		{
			if (hashTable.empty())
				return nullptr;

			HashValue hashMask = GetHashMask();
			HashValue masked = hash & hashMask;
			for (U32 i = 0; i < depth; i++)
			{
				if (hashTable[masked] != nullptr && GetItemHash(hashTable[masked]) == hash)
				{
					T* ret = hashTable[masked];
					hashTable[masked] = nullptr;
					list.erase(ret);
					return ret;
				}

				masked = (masked + 1) & hashMask;
			}
			return nullptr;
		}

		T* erase(T* value)
		{
			return erase(GetItemHash(value));
		}

		const IntrusiveList<T>& GetList()const
		{
			return list;
		}

		IntrusiveList<T>& GetList()
		{
			return list;
		}

		typename IntrusiveList<T>::Iterator begin() const
		{
			return list.begin();
		}
		typename IntrusiveList<T>::Iterator end() const
		{
			return list.end();
		}

	private:
		HashValue GetHashMask()const
		{
			return hashTable.size() - 1;
		}

		U32 ProbeDistanceHash(HashValue hash, HashValue curPos)
		{
			HashValue hashMask = GetHashMask();
			return (curPos - (hash & hashMask) + hashTable.size()) & hashMask;
		}

		inline HashValue GetItemHash(const T* value) const
		{
			return value != nullptr ? static_cast<const IntrusiveHashMapEnabled<T> *>(value)->GetHash() : ~0u;
		}

		bool InsertInner(T* value)
		{
			HashValue hashMask = GetHashMask();
			HashValue hash = GetItemHash(value);
			HashValue masked = hash & hashMask;
			for (U32 i = 0; i < depth; i++)
			{
				if (hashTable[masked] == nullptr)
				{
					hashTable[masked] = value;
					return true;
				}
				masked = (masked + 1) & hashMask;
			}
			return false;
		}

		void Grow()
		{
			bool success = false;
			do
			{
				for (auto& v : hashTable)
					v = nullptr;

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

				success = true;
				if (!list.empty())
				{
					for (auto& item : list)
					{
						if (!InsertInner(&item))
						{
							success = false;
							break;
						}
					}
				}
			} while (success == false);
		}

		std::vector<T*> hashTable;
		IntrusiveList<T> list;
		U32 depth = 0;
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
			auto itr = list.begin();
			while (itr != list.end())
			{
				auto* toMove = itr.get();
				readWrite.erase(toMove);

				T* toDelete = readOnly.insert_yield(toMove);
				if (toDelete != nullptr)
					pool.free(toDelete);

				itr = list.begin();
			}
		}

		void clear()
		{
			lock.BeginWrite();
			auto ClearInnerList = [&](IntrusiveList<T>& list) {
				auto itr = list.begin();
				while (itr != list.end())
				{
					auto* toFree = itr.get();
					itr = list.erase(itr);
					pool.free(toFree);
				}
			};
			ClearInnerList(readOnly.GetList());
			ClearInnerList(readWrite.GetList());
			readOnly.clear();
			readWrite.clear();
			lock.EndWrite();
		}

		T* find(HashValue hash) const
		{
			T* ret = readOnly.find(hash);
			if (ret != nullptr)
				return ret;

			lock.BeginRead();
			ret = readWrite.find(hash);
			lock.EndRead();
			return ret;
		}

		T& operator[](HashValue hash)
		{
			auto* t = find(hash);
			if (!t)
				t = emplace(hash);
			return *t;
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
			lock.BeginWrite();
			T* ret = pool.allocate(std::forward<Args>(args)...);
			lock.EndWrite();
			return ret;
		}

		void free(T* value)
		{
			lock.BeginWrite();
			pool.free(value);
			lock.EndWrite();
		}

		T* insert(HashValue hash, T* value)
		{
			SetItemHash(value, hash);
			lock.BeginWrite();
			T* old = readWrite.insert_yield(value);
			if (old != nullptr)
				pool.free(old);
			lock.EndWrite();
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
			static_cast<IntrusiveHashMapEnabled<T>*>(value)->SetHash(hash);
		}

		IntrusiveHashMapHolder<T> readOnly;
		IntrusiveHashMapHolder<T> readWrite;

		ObjectPool<T> pool;
		mutable RWLock lock;
	};

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
			auto& list = hashmap.GetList();
			auto itr = list.begin();
			while (itr != list.end())
			{
				auto* to_free = itr.get();
				itr = list.erase(itr);
				pool.free(to_free);
			}

			hashmap.clear();
		}

		T* find(HashValue hash) const
		{
			return hashmap.find(hash);
		}

		T& operator[](HashValue hash)
		{
			auto* t = find(hash);
			if (!t)
				t = emplace_yield(hash);
			return *t;
		}

		void erase(T* value)
		{
			hashmap.erase(value);
			pool.free(value);
		}

		void erase(HashValue hash)
		{
			auto* value = hashmap.erase(hash);
			if (value)
				pool.free(value);
		}

		template <typename... P>
		T* emplace_replace(HashValue hash, P&&... p)
		{
			T* t = allocate(std::forward<P>(p)...);
			return insert_replace(hash, t);
		}

		template <typename... P>
		T* emplace_yield(HashValue hash, P&&... p)
		{
			T* t = allocate(std::forward<P>(p)...);
			return insert_yield(hash, t);
		}

		template <typename... P>
		T* allocate(P&&... p)
		{
			return pool.allocate(std::forward<P>(p)...);
		}

		void free(T* value)
		{
			pool.free(value);
		}

		T* insert_replace(HashValue hash, T* value)
		{
			static_cast<IntrusiveHashMapEnabled<T>*>(value)->SetHash(hash);
			T* to_delete = hashmap.insert_replace(value);
			if (to_delete)
				pool.free(to_delete);
			return value;
		}

		T* insert_yield(HashValue hash, T* value)
		{
			static_cast<IntrusiveHashMapEnabled<T>*>(value)->SetHash(hash);
			T* to_delete = hashmap.insert_yield(value);
			if (to_delete)
				pool.free(to_delete);
			return value;
		}

		typename IntrusiveList<T>::Iterator begin() const
		{
			return hashmap.begin();
		}

		typename IntrusiveList<T>::Iterator end() const
		{
			return hashmap.end();
		}

		IntrusiveHashMap& get_thread_unsafe()
		{
			return *this;
		}

		const IntrusiveHashMap& get_thread_unsafe() const
		{
			return *this;
		}

	private:
		IntrusiveHashMapHolder<T> hashmap;
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
			hashmap.clear();
		}

		T* find(HashValue hash) const
		{
			ScopedReadLock holder(lock);
			T* ret = hashmap.find(hash);
			return ret;
		}

		void erase(T* value)
		{
			ScopedWriteLock holder(lock);
			hashmap.erase(value);
		}

		void erase(HashValue hash)
		{
			ScopedWriteLock holder(lock);
			hashmap.erase(hash);
		}

		template <typename... Args>
		T* allocate(Args&&... args)
		{
			ScopedWriteLock holder(lock);
			return hashmap.allocate(std::forward<Args>(args)...);
		}

		void free(T* value)
		{
			ScopedWriteLock holder(lock);
			hashmap.free(value);
		}

		T* insert(HashValue hash, T* value)
		{
			ScopedWriteLock holder(lock);
			value = hashmap.insert_replace(hash, value);
			return value;
		}

		template <typename... Args>
		T* emplace(HashValue hash, Args&&... args)
		{
			ScopedWriteLock holder(lock);
			return hashmap.emplace(hash, std::forward<Args>(args)...);
		}

	private:
		IntrusiveHashMap<T> hashmap;
		mutable RWLock lock;
	};
}
}