
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
		mainStats.deltaTimes = editor.GetLastDeltaTime();
		mainStats.updateTimes = editor.GetUpdateTime();
		mainStats.drawTimes = editor.GetDrawTime();
		mainStats.memoryCPU = Platform::GetProcessMemoryStats().usedPhysicalMemory;
		mainStats.memoryGPU = Renderer::GetDevice()->GetMemoryUsage().usage;
		ProfilerGPU::GetLastFrameData(mainStats.drawTimesGPU);

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

		// Get the last resolved GPU frame blocks
		gpuBlocks.clear();
		U64 maxFrame = 0;
		I32 maxFrameIndex = -1;
		auto buffers = ProfilerGPU::GetBlockBuffers();
		for (I32 index = 0; index < buffers.length(); index++)
		{
			auto& buffer = buffers[index];
			if (buffer.HasData() && buffer.frameIndex > maxFrame)
			{
				maxFrame = buffer.frameIndex;
				maxFrameIndex = index;
			}
		}

		if (maxFrameIndex != -1)
		{
			auto& buffer = buffers[maxFrameIndex];
			for (const auto& block : buffer.blocks)
				gpuBlocks.push_back(block);
		}
	}
}
}

#endif