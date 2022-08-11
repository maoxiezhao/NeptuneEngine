#ifdef CJING3D_PLATFORM_WIN32

#include "platform\sync.h"
#include "platform\platform.h"
#include "platform\atomic.h"
#include "utils\profiler.h"

#include <intrin.h>
#include <thread>
#include <mutex>

namespace VulkanTest
{

	Mutex::Mutex()
	{
		static_assert(sizeof(data) >= sizeof(SRWLOCK), "Data is too small for SRWLOCK");
		static_assert(alignof(Mutex) == alignof(SRWLOCK), "Alignment does not match");
		memset(data, 0, sizeof(data));
		SRWLOCK* lock = new(data) SRWLOCK;
		::InitializeSRWLock(lock);
	}

	Mutex::~Mutex()
	{
		SRWLOCK* lock = (SRWLOCK*)data;
		lock->~SRWLOCK();
	}

	void Mutex::Lock()
	{
		SRWLOCK* lock = (SRWLOCK*)data;
		::AcquireSRWLockExclusive(lock);
	}

	void Mutex::Unlock()
	{
		SRWLOCK* lock = (SRWLOCK*)data;
		::ReleaseSRWLockExclusive(lock);
	}

	Semaphore::Semaphore(I32 initialCount, I32 maximumCount, const char* debugName_)
	{
		id = ::CreateSemaphore(nullptr, initialCount, maximumCount, nullptr);
#ifdef DEBUG
		debugName = debugName;
#endif
	}

	Semaphore::~Semaphore()
	{
		::CloseHandle(id);
	}

	void Semaphore::Signal()
	{
		::ReleaseSemaphore(id, 1, nullptr);
	}

	void Semaphore::Wait()
	{
		::WaitForSingleObject(id, INFINITE);
	}

	ConditionVariable::ConditionVariable()
	{
		static_assert(sizeof(implData) >= sizeof(CONDITION_VARIABLE), "Size is not enough");
		static_assert(alignof(CONDITION_VARIABLE) == alignof(CONDITION_VARIABLE), "Alignment does not match");
		memset(implData, 0, sizeof(implData));
		CONDITION_VARIABLE* cv = new (implData) CONDITION_VARIABLE();
		::InitializeConditionVariable(cv);
	}

	ConditionVariable::~ConditionVariable()
	{
		((CONDITION_VARIABLE*)implData)->~CONDITION_VARIABLE();
	}

	void ConditionVariable::Sleep(Mutex& lock)
	{
		::SleepConditionVariableSRW((CONDITION_VARIABLE*)implData, (SRWLOCK*)lock.data, INFINITE, 0);
	}

	ConditionVariable::ConditionVariable(ConditionVariable&& rhs)
	{
		std::swap(implData, rhs.implData);
	}

	void ConditionVariable::Wakeup()
	{
		::WakeConditionVariable((CONDITION_VARIABLE*)implData);
	}

	void ConditionVariable::WakupAll()
	{
		::WakeAllConditionVariable((CONDITION_VARIABLE*)implData);
	}

	struct ThreadImpl
	{
		DWORD threadID = 0;
		HANDLE threadHandle = 0;
		Thread* owner;
		U64 affinityMask;
		U32 priority;
		bool isRunning = false;
		const char* name = nullptr;
		ConditionVariable cv;
	};

	static DWORD WINAPI ThreadEntryPoint(LPVOID lpThreadParameter)
	{
		ThreadImpl* impl = reinterpret_cast<ThreadImpl*>(lpThreadParameter);
		if (impl == nullptr) {
			return 0;
		}

#ifdef DEBUG
		// set thread debug name if in debug mode
		if (::IsDebuggerPresent() && !impl->name)
		{
#pragma pack(push, 8)
			typedef struct tagTHREADNAME_INFO
			{
				DWORD dwType;     /* must be 0x1000 */
				LPCSTR szName;    /* pointer to name (in user addr space) */
				DWORD dwThreadID; /* thread ID (-1=caller thread) */
				DWORD dwFlags;    /* reserved for future use, must be zero */
			} THREADNAME_INFO;
#pragma pack(pop)
			THREADNAME_INFO info;
			memset(&info, 0, sizeof(info));
			info.dwType = 0x1000;
			info.szName = impl->name;
			info.dwThreadID = (DWORD)-1;
			info.dwFlags = 0;

			const DWORD MS_VC_EXCEPTION = 0x406D1388;
			::RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG), (const ULONG_PTR*)&info);
		}
#endif
		Profiler::SetThreadName(impl->name);
		int ret = impl->owner->Task();
		impl->isRunning = false;
		return ret;
	}

	Thread::Thread()
	{
		impl = CJING_NEW(ThreadImpl);
		impl->priority = ::GetThreadPriority(::GetCurrentThread());
		impl->owner = this;
		impl->name = nullptr;
		impl->isRunning = false;
	}

	Thread::~Thread()
	{
		ASSERT(!impl->threadHandle);
		CJING_SAFE_DELETE(impl);
	}

	void Thread::SetAffinity(U64 mask)
	{
		ASSERT(impl != nullptr);
		impl->affinityMask = mask;
		::SetThreadAffinityMask(impl->threadHandle, mask);
	}

	bool Thread::IsValid() const
	{
		return impl != nullptr;
	}

	void Thread::Sleep(Mutex& lock)
	{
		ASSERT(impl != nullptr);
		impl->cv.Sleep(lock);
	}

	void Thread::Wakeup()
	{
		ASSERT(impl != nullptr);
		impl->cv.Wakeup();
	}

	bool Thread::IsFinished() const
	{
		return !impl->isRunning;
	}

	static constexpr U32 STACK_SIZE = 0x8000;
	bool Thread::Create(const char* name)
	{
		impl->name = name;

		HANDLE handle = ::CreateThread(nullptr, STACK_SIZE, ThreadEntryPoint, impl, CREATE_SUSPENDED, &impl->threadID);
		if (handle)
		{
			impl->threadHandle = handle;
			impl->isRunning = true;

			bool success = ::ResumeThread(impl->threadHandle) != -1;
			if (success)
				return true;

			::CloseHandle(impl->threadHandle);
			impl->threadHandle = nullptr;
		}
		return false;
	}

	void Thread::Destroy()
	{
		if (impl != nullptr)
		{
			while (impl->isRunning)
				Platform::Sleep(1);

			::CloseHandle(impl->threadHandle);
			impl->threadHandle = nullptr;
		}
	}

	void Thread::Join()
	{
		WaitForSingleObject(impl->threadHandle, INFINITE);
	}

	struct RWLockImpl
	{
		SRWLOCK lock = SRWLOCK_INIT;
	};

	RWLockImpl* RWLock::Get()
	{
		return reinterpret_cast<RWLockImpl*>(&data[0]);
	}

	RWLock::RWLock()
	{
		memset(data, 0, sizeof(data));
		new(data) RWLockImpl();
	}

	RWLock::~RWLock()
	{
	}

	void RWLock::BeginRead()
	{
		::AcquireSRWLockShared(&Get()->lock);
	}

	void RWLock::EndRead()
	{
		::ReleaseSRWLockShared(&Get()->lock);
	}

	void RWLock::BeginWrite()
	{
		::AcquireSRWLockExclusive(&Get()->lock);
	}

	void RWLock::EndWrite()
	{
		::ReleaseSRWLockExclusive(&Get()->lock);
	}
}

#endif