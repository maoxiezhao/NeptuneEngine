#pragma once

#include "core\common.h"
#include "core\platform\platform.h"
#include "core\utils\path.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Globals
	{
	public:
		static bool IsRequestingExit;
		static I32  ExitCode;
		static bool IsFatal;

		static Platform::ThreadID MainThreadID;
	
		static String ProductName;

		static Path StartupFolder;
#ifdef CJING3D_EDITOR
		static Path EngineContentFolder;
#endif
		static Path ProjectFolder;
		static Path ProjectContentFolder;
		static Path ProjectCacheFolder;
	};

	bool IsInMainThread();
}