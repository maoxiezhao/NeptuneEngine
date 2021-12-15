#ifdef CJING3D_PLATFORM_WIN32

#include "platform\sync.h"
#include "platform\platform.h"
#include "platform\atomic.h"

#include <intrin.h>
#include <thread>
#include <mutex>

namespace VulkanTest
{
	struct MutexImpl
	{
		CRITICAL_SECTION critSec;
		HANDLE lockedThread;
		volatile I32 lockedCount = 0;
	};

	MutexImpl* Mutex::Get()
	{
		return reinterpret_cast<MutexImpl*>(&data[0]);
	}

	Mutex::Mutex()
	{
		static_assert(sizeof(MutexImpl) <= sizeof(data), "data too small for MutexImpl!");
		memset(data, 0, sizeof(data));
		new(data) MutexImpl();
		::InitializeCriticalSection(&Get()->critSec);
	}

	Mutex::~Mutex()
	{
		::DeleteCriticalSection(&Get()->critSec);
		Get()->~MutexImpl();
	}

	Mutex::Mutex(Mutex&& rhs)
	{
		rhs.Lock();
		std::swap(data, rhs.data);
		Unlock();
	}

	void Mutex::operator=(Mutex&& rhs)
	{
		rhs.Lock();
		std::swap(data, rhs.data);
		Unlock();
	}

	void Mutex::Lock()
	{
		ASSERT(Get() != nullptr);
		::EnterCriticalSection(&Get()->critSec);
		if (AtomicIncrement(&Get()->lockedCount) == 1) {
			Get()->lockedThread = ::GetCurrentThread();
		}
	}

	bool Mutex::TryLock()
	{
		ASSERT(Get() != nullptr);
		if (!!::TryEnterCriticalSection(&Get()->critSec))
		{
			if (AtomicIncrement(&Get()->lockedCount) == 1) {
				Get()->lockedThread = ::GetCurrentThread();
			}
			return true;
		}
		return false;
	}

	void Mutex::Unlock()
	{
		ASSERT(Get() != nullptr);
		ASSERT(Get()->lockedThread == ::GetCurrentThread());
		if (AtomicDecrement(&Get()->lockedCount) == 0) {
			Get()->lockedThread = nullptr;
		}
		::LeaveCriticalSection(&Get()->critSec);
	}

	struct SemaphoreImpl
	{
		HANDLE handle_;
	};

	SemaphoreImpl* Semaphore::Get()
	{
		return reinterpret_cast<SemaphoreImpl*>(&data[0]);
	}

	Semaphore::Semaphore(I32 initialCount, I32 maximumCount, const char* debugName_)
	{
		memset(data, 0, sizeof(data));
		new(data) SemaphoreImpl();
		Get()->handle_ = ::CreateSemaphore(nullptr, initialCount, maximumCount, nullptr);
#ifdef DEBUG
		debugName = debugName;
#endif
	}

	Semaphore::Semaphore(Semaphore&& rhs)
	{
		std::swap(data, rhs.data);
#ifdef DEBUG
		std::swap(debugName, rhs.debugName);
#endif
	}

	Semaphore::~Semaphore()
	{
		::CloseHandle(Get()->handle_);
		Get()->~SemaphoreImpl();
	}

	bool Semaphore::Signal(I32 count)
	{
		ASSERT(Get() != nullptr);
		return ::ReleaseSemaphore(Get()->handle_, count, nullptr);
	}

	bool Semaphore::Wait()
	{
		ASSERT(Get() != nullptr);
		return (::WaitForSingleObject(Get()->handle_, INFINITE) == WAIT_OBJECT_0);
	}
}

#endif