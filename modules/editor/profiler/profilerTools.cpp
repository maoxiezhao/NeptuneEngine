
#ifdef CJING3D_EDITOR

#include "profilerTools.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	ProfilerTools::ProfilerTools(EditorApp& editor_) :
		editor(editor_)
	{
	}

	ProfilerTools::~ProfilerTools()
	{
	}

	void ProfilerTools::Update()
	{
		// Get main stats
		mainStats.fps = (I32)editor.GetFPS();
		mainStats.updateTimes = editor.GetLastDeltaTime();

			// Get cpu profiler blocks
		auto & threads = Profiler::GetThreads();
		for (auto& thread : threads)
		{
			if (thread == nullptr)
				continue;

			ThreadStats* stats = nullptr;
			for (auto& s : cpuThreads)
			{
				if (EqualString(s.name, thread->name.c_str()))
				{
					stats = &s;
					break;
				}
			}
			if (stats == nullptr)
			{
				cpuThreads.emplace_back();
				stats = &cpuThreads.back();
				stats->name = thread->name.c_str();
			}

			thread->GetBlocks(stats->blocks);
		}
	}
}
}

#endif