#pragma once

#include "core\common.h"
#include "core\profiler\profiler.h"
#include "core\profiler\renderStats.h"
#include "gpu\vulkan\commandList.h"
#include "gpu\vulkan\queryPool.h"

// Because of the module dependency(Core->GPU) profilerGPU cannot be implemented in core\profiler.h 
// This problem will be solved for the future.

namespace VulkanTest
{
namespace ProfilerGPU
{
	struct Block
	{
		String name;
		F32 startTime;
		F32 time;
		RenderStats stats;

		VkPipelineStageFlagBits stage;
		GPU::CommandList* cmd;
		GPU::QueryPoolResultPtr gpuBegin;
		GPU::QueryPoolResultPtr gpuEnd;
		I32 depth;
	};

	struct BlockBuffer
	{
		bool isResolved = false;
		U64 frameIndex;
		std::vector<Block> blocks;

	public:
		I32 Add(const Block& block);
		Block* Get(I32 index);
		void Clear();
		void TryResolve();
		bool HasData() const;
	};

	void BeginFrame();
	void EndFrame();
	I32  BeginBlockGPU(const char* name, GPU::CommandList& cmd, VkPipelineStageFlagBits stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	void EndBlockGPU(I32 id);
	I32  BeginBlockRenderPass(const char* name, GPU::CommandList& cmd, VkPipelineStageFlagBits stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	void EndBlockRenderPass(I32 id);
	Span<BlockBuffer> GetBlockBuffers();
	bool GetLastFrameData(F32& drawTimeMs);

	struct Scope
	{
		I32 index;

		Scope(const char* name, GPU::CommandList& cmd) {
			index = BeginBlockGPU(name, cmd);
		}

		~Scope() {
			EndBlockGPU(index);
		}
	};
}

#define PROFILER_GPU_CONCAT2(a, b) a ## b
#define PROFILER_GPU_CONCAT(a, b) PROFILER_GPU_CONCAT2(a, b)
#define PROFILE_GPU(name, cmd) ProfilerGPU::Scope PROFILER_GPU_CONCAT(profile_scope, __LINE__)(name, cmd);
}