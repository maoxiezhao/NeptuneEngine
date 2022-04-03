#pragma once

#include "core\common.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Timer 
	{
	public:
		Timer();

		F32 Tick();
		F32 GetTimeSinceStart();
		F32 GetTotalDeltaTime();

	private:
		U64 frequency;
		U64 lastTick;
		U64 firstTick;
		F32 totalDeltaTime;
	};
}