#pragma once

#include "core\common.h"

namespace VulkanTest
{
	I32 AtomicStore(volatile I32* pw, I32 val);
	I32 AtomicDecrement(volatile I32* pw);
	I32 AtomicIncrement(volatile I32* pw);
	I32 AtomicAdd(volatile I32* pw, volatile I32 val);
	I32 AtomicAddAcquire(volatile I32* pw, volatile I32 val);
	I32 AtomicAddRelease(volatile I32* pw, volatile I32 val);
	I32 AtomicSub(volatile I32* pw, volatile I32 val);
	I32 AtomicExchange(volatile I32* pw, I32 exchg);
	I32 AtomicCmpExchange(volatile I32* pw, I32 exchg, I32 comp);
	I32 AtomicCmpExchangeAcquire(volatile I32* pw, I32 exchg, I32 comp);
	I32 AtomicExchangeIfGreater(volatile I32* pw, volatile I32 val);
	I32 AtomicRead(volatile I32* pw);

	I32 AtomicStore(volatile I64* pw, I64 val);
	I64 AtomicDecrement(volatile I64* pw);
	I64 AtomicIncrement(volatile I64* pw);
	I64 AtomicAdd(volatile I64* pw, volatile I64 val);
	I64 AtomicAddAcquire(volatile I64* pw, volatile I64 val);
	I64 AtomicAddRelease(volatile I64* pw, volatile I64 val);
	I64 AtomicSub(volatile I64* pw, volatile I64 val);
	I64 AtomicExchange(volatile I64* pw, I64 exchg);
	I64 AtomicCmpExchange(volatile I64* pw, I64 exchg, I64 comp);
	I64 AtomicCmpExchangeAcquire(volatile I64* pw, I64 exchg, I64 comp);
	I64 AtomicExchangeIfGreater(volatile I64* pw, volatile I64 val);
	I64 AtomicRead(volatile I64* pw);
}