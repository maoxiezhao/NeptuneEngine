//#pragma once
//
//#include "math\hash.h"
//#include "core\utils\objectPool.h"
//
//#include <unordered_map>
//
//namespace VulkanTest
//{
//namespace GPU
//{
//namespace Tools
//{ 
//	template <typename T>
//	class VulkanCache
//	{
//	public:
//		~VulkanCache()
//		{
//			clear();
//		}
//
//		VulkanCache() = default;
//		VulkanCache(const VulkanCache&) = delete;
//		void operator=(const VulkanCache&) = delete;
//
//		void clear()
//		{
//			for (auto& kvp : hashmap)
//				pool.free(kvp.second);
//
//			hashmap.clear();
//		}
//
//		T* find(HashValue hash) const
//		{
//			auto it = hashmap.find(hash);
//			if (it == hashmap.end())
//				return nullptr;
//
//			return it->second;
//		}
//
//		T& operator[](HashValue hash)
//		{
//			auto* t = find(hash);
//			if (!t)
//				t = emplace(hash);
//			return *t;
//		}
//
//		void erase(T* value)
//		{
//			for (auto& kvp : hashmap)
//			{
//				if (kvp.second == value)
//				{
//					hashmap.erase(kvp.first);
//					pool.free(value);
//					break;
//				}
//			}
//		}
//
//		void erase(HashValue hash)
//		{
//			auto it = hashmap.find(hash);
//			if (it != hashmap.end())
//			{
//				pool.free(it->second);
//				hashmap.erase(it);
//			}
//		}
//
//		template <typename... Args>
//		T* emplace(HashValue hash, Args&&... args)
//		{
//			T* t = allocate(std::forward<Args>(args)...);
//			return insert(hash, t);
//		}
//
//		template <typename... Args>
//		T* allocate(Args&&... args)
//		{
//			return pool.allocate(std::forward<Args>(args)...);
//		}
//
//		void free(T* value)
//		{
//			pool.free(value);
//		}
//
//		T* insert(HashValue hash, T* value)
//		{
//			auto it = hashmap.find(hash);
//			if (it != hashmap.end())
//				pool.free(it->second);
//
//			hashmap[hash] = value;
//			return value;
//		}
//
//		typename std::unordered_map<HashValue, T*>::const_iterator begin() const
//		{
//			return hashmap.begin();
//		}
//
//		typename std::unordered_map<HashValue, T*>::const_iterator end() const
//		{
//			return hashmap.end();
//		}
//
//	private:
//		std::unordered_map<HashValue, T*> hashmap;
//		Util::ObjectPool<T> pool;
//	};
//
//
//	template <typename T>
//	class ThreadSafeVulkanCache
//	{
//	public:
//		~ThreadSafeVulkanCache()
//		{
//			clear();
//		}
//
//		ThreadSafeVulkanCache() = default;
//		ThreadSafeVulkanCache(const ThreadSafeVulkanCache&) = delete;
//		void operator=(const ThreadSafeVulkanCache&) = delete;
//
//		void clear()
//		{
//			for (auto& kvp : hashmap)
//				pool.free(kvp.second);
//
//			hashmap.clear();
//		}
//
//		T* find(HashValue hash) const
//		{
//			auto it = hashmap.find(hash);
//			if (it == hashmap.end())
//				return nullptr;
//
//			return it->second;
//		}
//
//		T& operator[](HashValue hash)
//		{
//			auto* t = find(hash);
//			if (!t)
//				t = emplace(hash);
//			return *t;
//		}
//
//		void erase(T* value)
//		{
//			for (auto& kvp : hashmap)
//			{
//				if (kvp.second == value)
//				{
//					hashmap.erase(kvp.first);
//					pool.free(value);
//					break;
//				}
//			}
//		}
//
//		void erase(HashValue hash)
//		{
//			auto it = hashmap.find(hash);
//			if (it != hashmap.end())
//			{
//				pool.free(it->second);
//				hashmap.erase(it);
//			}
//		}
//
//		template <typename... Args>
//		T* emplace(HashValue hash, Args&&... args)
//		{
//			T* t = allocate(std::forward<Args>(args)...);
//			return insert(hash, t);
//		}
//
//		template <typename... Args>
//		T* allocate(Args&&... args)
//		{
//			return pool.allocate(std::forward<Args>(args)...);
//		}
//
//		void free(T* value)
//		{
//			pool.free(value);
//		}
//
//		T* insert(HashValue hash, T* value)
//		{
//			auto it = hashmap.find(hash);
//			if (it != hashmap.end())
//				pool.free(it->second);
//
//			hashmap[hash] = value;
//			return value;
//		}
//
//
//	private:
//		std::unordered_map<HashValue, T*> hashmap;
//		Util::ObjectPool<T> pool;
//		RWLock lock;
//	};
//}
//}
//}