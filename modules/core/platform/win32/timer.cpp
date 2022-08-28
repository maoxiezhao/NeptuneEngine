#ifdef CJING3D_PLATFORM_WIN32

#include "platform\platform.h"
#include "platform\timer.h"

namespace VulkanTest
{
	Timer::Timer() :
		totalDeltaTime(0.0f)
	{
		LARGE_INTEGER f, n;
		QueryPerformanceFrequency(&f);
		QueryPerformanceCounter(&n);
		firstTick = lastTick = n.QuadPart;
		frequency = f.QuadPart;
	}

	F32 Timer::Tick()
	{
		LARGE_INTEGER n;
		QueryPerformanceCounter(&n);
		const U64 tick = n.QuadPart;
		F32 delta = static_cast<F32>((F64)(tick - lastTick) / (F64)frequency);
		lastTick = tick;
		totalDeltaTime += delta;
		return delta;
	}

	F32 Timer::GetTimeSinceStart()
	{
		LARGE_INTEGER n;
		QueryPerformanceCounter(&n);
		const U64 tick = n.QuadPart;
		return static_cast<F32>((F64)(tick - firstTick) / (F64)frequency);
	}

	F32 Timer::GetTimeSinceTick()
	{
		LARGE_INTEGER n;
		QueryPerformanceCounter(&n);
		const U64 tick = n.QuadPart;
		return static_cast<F32>((F64)(tick - lastTick) / (F64)frequency);
	}

	F32 Timer::GetTotalDeltaTime()
	{
		return totalDeltaTime;
	}

	U64 Timer::GetRawTimestamp()
	{
		LARGE_INTEGER tick;
		QueryPerformanceCounter(&tick);
		return tick.QuadPart;
	}

	F64 Timer::GetTimeSeconds()
	{
		LARGE_INTEGER tick;
		QueryPerformanceCounter(&tick);
		return (F64)(tick.QuadPart) / (F64)GetFrequency();
	}

	U64 Timer::GetFrequency()
	{
		static U64 freq = []() {
			LARGE_INTEGER f;
			QueryPerformanceFrequency(&f);
			return f.QuadPart;
		}();
		return freq;
	}
}

#endif