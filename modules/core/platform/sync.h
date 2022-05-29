#pragma once

#include "core\common.h"

#include <functional>

#ifndef CJING3D_PLATFORM_WIN32
#include <thread>
#include <mutex>
#else
#include <thread>
#endif

namespace VulkanTest
{
	struct alignas(8) VULKAN_TEST_API Mutex
	{
	public:
		friend class ConditionVariable;

		Mutex();
		~Mutex();

		void Lock();
		void Unlock();

	private:
		Mutex(const Mutex& rhs) = delete;
		Mutex& operator=(const Mutex& rhs) = delete;

		U8 data[8];
	};

	class VULKAN_TEST_API ScopedMutex
	{
	public:
		ScopedMutex(Mutex& mutex_) :
			mutex(mutex_)
		{
			mutex.Lock();
		}

		~ScopedMutex()
		{
			mutex.Unlock();
		}

	private:
		ScopedMutex(const Mutex& rhs) = delete;
		ScopedMutex(Mutex&& rhs) = delete;

		Mutex& mutex;
	};

	class VULKAN_TEST_API Semaphore
	{
	public:
		Semaphore(I32 initialCount, I32 maximumCount, const char* debugName_ = nullptr);
		~Semaphore();
		Semaphore(const Semaphore&) = delete;

		void Signal();
		void Wait();

	private:
		void* id;

#ifdef DEBUG
		const char* debugName = nullptr;
#endif
	};

	class VULKAN_TEST_API ConditionVariable
	{
	public:
		ConditionVariable();
		ConditionVariable(ConditionVariable&& rhs);
		~ConditionVariable();

		void Sleep(Mutex& lock);
		void Wakeup();

	private:
		ConditionVariable(const ConditionVariable&) = delete;

		U8 implData[64];
	};

	class VULKAN_TEST_API Thread
	{
	public:
		static const I32 DEFAULT_STACK_SIZE = 0x8000;

		Thread();
		~Thread();

		void SetAffinity(U64 mask);
		bool IsValid()const;
		void Sleep(Mutex& lock);
		void Wakeup();
		bool IsFinished()const;
		bool Create(const char* name);
		void Destroy();

		virtual int Task() { return 0; };

	private:
		Thread(const Thread& rhs) = delete;
		Thread& operator=(const Thread& rhs) = delete;

		struct ThreadImpl* impl = nullptr;
	};

	class VULKAN_TEST_API RWLock final
	{
	public:
		RWLock();
		~RWLock();

		void BeginRead();
		void EndRead();
		void BeginWrite();
		void EndWrite();

	private:
		friend class ConditionVariable;

		RWLock(const RWLock&) = delete;

		struct RWLockImpl* Get();
		mutable U8 data[8];
	};

	class VULKAN_TEST_API ScopedReadLock final
	{
	public:
		ScopedReadLock(RWLock& lock_)
			: lock(lock_)
		{
			lock.BeginRead();
		}

		~ScopedReadLock() { lock.EndRead(); }

	private:
		RWLock& lock;
	};

	class VULKAN_TEST_API ScopedWriteLock final
	{
	public:
		ScopedWriteLock(RWLock& lock_)
			: lock(lock_)
		{
			lock.BeginWrite();
		}

		~ScopedWriteLock() { lock.EndWrite(); }

	private:
		RWLock& lock;
	};

}