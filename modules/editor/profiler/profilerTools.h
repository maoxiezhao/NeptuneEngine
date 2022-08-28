#pragma once

#ifdef CJING3D_EDITOR

#include "core\common.h"
#include "core\collections\array.h"
#include "core\utils\string.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	class ProfilerTools
	{
	public:
		struct MainStats
		{
			I32 fps;
			F32 updateTimes;
		};

		struct ThreadStats
		{
			String name;
			std::vector<Profiler::Block> blocks;
		};

		ProfilerTools(EditorApp& editor_);
		~ProfilerTools();

		void Update();

		MainStats GetMainStats()const {
			return mainStats;
		}

		const std::vector<ThreadStats>& GetCPUThreads() const {
			return cpuThreads;
		}

	private:
		MainStats mainStats;
		std::vector<ThreadStats> cpuThreads;
		EditorApp& editor;
	};
}
}

#endif