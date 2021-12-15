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
	class VULKAN_TEST_API Mutex
	{
	public:
		Mutex();
		~Mutex();
		Mutex(Mutex&& rhs);
		void operator=(Mutex&& rhs);

		void Lock();
		bool TryLock();
		void Unlock();

	private:
		Mutex(const Mutex& rhs) = delete;
		Mutex& operator=(const Mutex& rhs) = delete;

		struct MutexImpl* Get();
		U8 data[60];
	};

	class ScopedMutex
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

	class Semaphore
	{
	public:
		Semaphore(I32 initialCount, I32 maximumCount, const char* debugName_ = nullptr);
		Semaphore(Semaphore&& rhs);
		~Semaphore();

		bool Signal(I32 count);
		bool Wait();

	private:
		Semaphore(const Semaphore&) = delete;

		struct SemaphoreImpl* Get();
		U8 data[32];

#ifdef DEBUG
		const char* debugName = nullptr;
#endif
	};
}