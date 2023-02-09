#pragma once

#include "core\common.h"
#include "core\platform\platform.h"
#include "core\platform\atomic.h"

namespace VulkanTest
{
	template<typename T, int MaxThreads = PLATFORM_THREADS_LIMIT>
	class ThreadLocal
	{
	protected:
		struct Bucket
		{
			volatile I64 threadID;
			T value;
		};
		Bucket buckets[MaxThreads];

	public:
		ThreadLocal()
		{
			memset(buckets, 0, sizeof(buckets));
		}

		T& Get()
		{
			return buckets[GetIndex()].value;
		}

		void Set(const T& value)
		{
			buckets[GetIndex()].value = value;
		}

		void GetValues(Array<T>& result) const
		{
			result.reserve(MaxThreads);
			for (I32 i = 0; i < MaxThreads; i++)
			{
				result.push_back(buckets[i].value);
			}
		}

	protected:
		FORCE_INLINE static I32 Hash(const I64 value)
		{
			return value & (MaxThreads - 1);
		}

		FORCE_INLINE I32 GetIndex()
		{
			I64 key = (I64)Platform::GetCurrentThreadID();
			auto index = Hash(key);
			while (true)
			{
				const I64 value = AtomicRead(&buckets[index].threadID);
				if (value == key)
					break;
				if (value == 0 && AtomicCmpExchange(&buckets[index].threadID, key, 0) == 0)
					break;
				index = Hash(index + 1);
			}
			return index;
		}
	};

	template<typename T, int MaxThreads = PLATFORM_THREADS_LIMIT>
	class ThreadLocalObject : public ThreadLocal<T*, MaxThreads>
	{
	public:
		typedef ThreadLocal<T*, MaxThreads> Base;

		void Delete()
		{
			auto value = Base::Get();
			CJING_SAFE_DELETE(value);
		}

		void DeleteAll()
		{
			for (I32 i = 0; i < MaxThreads; i++)
			{
				auto& bucket = Base::buckets[i];
				if (bucket.value != nullptr)
				{
					CJING_SAFE_DELETE(bucket.value);
					bucket.threadID = 0;
					bucket.value = nullptr;
				}
			}
		}

		void GetNotNullValues(Array<T*>& result) const
		{
			result.reserve(MaxThreads);
			for (I32 i = 0; i < MaxThreads; i++)
			{
				if (Base::buckets[i].value != nullptr)
					result.push_back(Base::buckets[i].value);
			}
		}
	};
}