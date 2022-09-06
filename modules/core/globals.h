#pragma once

#include "core\common.h"
#include "core\platform\platform.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Globals
	{
	public:
		static bool IsRequestingExit;
		static I32  ExitCode;

		static Platform::ThreadID MainThreadID;
	};

	bool IsInMainThread();
}