#include "globals.h"

namespace VulkanTest
{
	bool Globals::IsRequestingExit = false;
	bool Globals::IsFatal = false;

	I32 Globals::ExitCode;

	Platform::ThreadID Globals::MainThreadID;

	String Globals::ProductName;

	Path Globals::StartupFolder;
#ifdef CJING3D_EDITOR
	Path Globals::EngineContentFolder;
#endif
	Path Globals::ProjectFolder;
	Path Globals::ProjectContentFolder;
	Path Globals::ProjectCacheFolder;
	Path Globals::ProjectLocalFolder;

	bool IsInMainThread()
	{
		return Globals::MainThreadID == Platform::GetCurrentThreadID();
	}
}