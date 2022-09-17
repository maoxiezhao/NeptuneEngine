#include "platform\atomic.h"
#include "platform\platform.h"

/////////////////////////////////////////////////////////////////////////////////////////
// ATOMIC WIN32
////////////////////////////////////////////////////////////////////////////////////////
#ifdef CJING3D_PLATFORM_WIN32

namespace VulkanTest
{
//////////////////////////////////////////////////////////////////////////
// I32
//////////////////////////////////////////////////////////////////////////

	I32 AtomicStore(volatile I32* pw, I32 val)
	{
		return InterlockedExchange((LONG volatile*)pw, val);
	}

	I32 AtomicDecrement(volatile I32* pw)
	{
		return InterlockedDecrement((LONG volatile*)pw);
	}

	I32 AtomicIncrement(volatile I32* pw)
	{
		return InterlockedIncrement((LONG volatile*)pw);
	}

	I32 AtomicAdd(volatile I32* pw, volatile I32 val)
	{
		return InterlockedAdd((LONG volatile*)pw, val);
	}

	I32 AtomicAddAcquire(volatile I32* pw, volatile I32 val)
	{
		return InterlockedAddAcquire((LONG volatile*)pw, val);
	}

	I32 AtomicAddRelease(volatile I32* pw, volatile I32 val)
	{
		return InterlockedAddRelease((LONG volatile*)pw, val);
	}

	I32 AtomicSub(volatile I32* pw, volatile I32 val)
	{
		return InterlockedExchangeAdd((LONG volatile*)pw, -(int32_t)val) - val;
	}

	I32 AtomicExchange(volatile I32* pw, I32 exchg)
	{
		return InterlockedExchange((LONG volatile*)pw, exchg);
	}

	I32 AtomicCmpExchange(volatile I32* pw, I32 exchg, I32 comp)
	{
		return InterlockedCompareExchange((LONG volatile*)(pw), exchg, comp);
	}

	I32 AtomicCmpExchangeAcquire(volatile I32* pw, I32 exchg, I32 comp)
	{
		return InterlockedCompareExchangeAcquire((LONG volatile*)(pw), exchg, comp);
	}

	I32 AtomicExchangeIfGreater(volatile I32* pw, volatile I32 val)
	{
		while (true)
		{
			I32 tmp = static_cast<I32 const volatile&>(*(pw));
			if (tmp >= val) {
				return tmp;
			}

			if (InterlockedCompareExchange((LONG volatile*)(pw), val, tmp) == tmp) {
				return val;
			}
		}
	}
	I32 AtomicRead(volatile I32* pw)
	{
		return (I32)_InterlockedCompareExchange((long volatile*)pw, 0, 0);
	}

	//////////////////////////////////////////////////////////////////////////
	// I64
	//////////////////////////////////////////////////////////////////////////

	I64 AtomicStore(volatile I64* pw, I64 val)
	{
		return InterlockedExchange64((LONGLONG volatile*)pw, val);
	}

	I64 AtomicDecrement(volatile I64* pw)
	{
		return InterlockedDecrement64((LONGLONG volatile*)pw);
	}

	I64 AtomicIncrement(volatile I64* pw)
	{
		return InterlockedIncrement64((LONGLONG volatile*)pw);
	}

	I64 AtomicAdd(volatile I64* pw, volatile I64 val)
	{
		return InterlockedAdd64((LONGLONG volatile*)pw, val);
	}

	I64 AtomicAddAcquire(volatile I64* pw, volatile I64 val)
	{
		return InterlockedAddAcquire64((LONGLONG volatile*)pw, val);
	}

	I64 AtomicAddRelease(volatile I64* pw, volatile I64 val)
	{
		return InterlockedAddRelease64((LONGLONG volatile*)pw, val);
	}

	I64 AtomicSub(volatile I64* pw, volatile I64 val)
	{
		return InterlockedExchangeAdd64((LONGLONG volatile*)pw, -(int64_t)val) - val;
	}

	I64 AtomicExchange(volatile I64* pw, I64 exchg)
	{
		return InterlockedExchange64((LONGLONG volatile*)pw, exchg);
	}

	I64 AtomicCmpExchange(volatile I64* pw, I64 exchg, I64 comp)
	{
		return InterlockedCompareExchange64((LONGLONG volatile*)pw, exchg, comp);
	}

	I64 AtomicCmpExchangeAcquire(volatile I64* pw, I64 exchg, I64 comp)
	{
		return InterlockedCompareExchangeAcquire64((LONGLONG volatile*)pw, exchg, comp);
	}

	I64 AtomicExchangeIfGreater(volatile I64* pw, volatile I64 val)
	{
		while (true)
		{
			I64 tmp = static_cast<I64 const volatile&>(*(pw));
			if (tmp >= val) {
				return tmp;
			}

			if (InterlockedCompareExchange64((LONGLONG volatile*)(pw), val, tmp) == tmp) {
				return val;
			}
		}
	}

	I64 AtomicRead(volatile I64* pw)
	{
		return _InterlockedCompareExchange64(pw, 0, 0);
	}
}

#endif