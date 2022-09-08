#pragma once

#ifdef CJING3D_EDITOR

#include "core\common.h"
#include "core\collections\array.h"
#include "core\utils\string.h"
#include "core\profiler\profiler.h"
#include "renderer\profiler\profilerGPU.h"

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
			F32 deltaTimes;
			F32 updateTimes;
			F32 drawTimes;
			F32 drawTimesGPU;
			U64 memoryCPU;
			U64 memoryGPU;
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

		const std::vector<ProfilerGPU::Block> GetGPUBlocks()const {
			return gpuBlocks;
		}

	private:
		MainStats mainStats;
		std::vector<ThreadStats> cpuThreads;
		std::vector<ProfilerGPU::Block> gpuBlocks;
		EditorApp& editor;
	};
}
}

#endif