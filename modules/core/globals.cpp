#include "globals.h"

namespace VulkanTest
{
	bool Globals::IsRequestingExit = false;

	I32 Globals::ExitCode;

	Platform::ThreadID Globals::MainThreadID;

	bool IsInMainThread()
	{
		return Globals::MainThreadID == Platform::GetCurrentThreadID();
	}
}