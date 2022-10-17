#pragma once

#include "core\common.h"

#include <functional>
#include <thread>
#include <mutex>

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
		void Signal(U32 value);
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
		void WakupAll();

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
		void Join();

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

	class VULKAN_TEST_API SpinLock
	{
	public:
		inline void Lock()
		{
			int spin = 0;
			while (!TryLock())
			{
				if (spin < 10)
					_mm_pause(); // SMT thread swap can occur here
				else
					std::this_thread::yield(); // OS thread swap can occur here. It is important to keep it as fallback, to avoid any chance of lockup by busy wait
	
				spin++;
			}
		}
		inline bool TryLock()
		{
			return !lck.test_and_set(std::memory_order_acquire);
		}

		inline void Unlock()
		{
			lck.clear(std::memory_order_release);
		}

	private:
		std::atomic_flag lck = ATOMIC_FLAG_INIT;
	};

}