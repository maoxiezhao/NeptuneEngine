#include "profilerGPU.h"
#include "renderer.h"
#include "core\engine.h"

namespace VulkanTest
{
namespace ProfilerGPU
{
#define PROFILER_GPU_EVENTS_FRAMES (GPU::BUFFERCOUNT + 2)

	namespace ProfilerGPUImpl 
	{
		I32 depth = 0;
		Mutex mutex;
		I32 currentBuffer = 0;
		BlockBuffer blockBuffers[PROFILER_GPU_EVENTS_FRAMES];
	}
	using namespace ProfilerGPUImpl;

	class ProfilerServiceImpl : public EngineService
	{
	public:
		ProfilerServiceImpl() :
			EngineService("ProfilerService", -200)
		{}

		void BeforeUninit() override
		{
			for (I32 i = 0; i < ARRAYSIZE(blockBuffers); i++)
				blockBuffers[i].Clear();

			RenderStats::Counters.DeleteAll();
		}
	};
	ProfilerServiceImpl ProfilerServiceImplInstance;

	I32 BlockBuffer::Add(const Block& block)
	{
		const I32 index = (I32)blocks.size();
		blocks.push_back(block);
		return index;
	}

	Block* BlockBuffer::Get(I32 index)
	{
		return &blocks[index];
	}

	void BlockBuffer::Clear()
	{
		blocks.clear();
		frameIndex = 0;
		isResolved = false;
	}

	void BlockBuffer::TryResolve()
	{
		if (isResolved || blocks.empty())
			return;

		// Check all queries are ready
		for (I32 i = (I32)blocks.size() - 1; i >= 0; i--)
		{
			if (!blocks[i].gpuBegin->HasTimesteamp() || !blocks[i].gpuEnd->HasTimesteamp())
				return;
		}

		auto device = Renderer::GetDevice();
		F64 frequency = (F64)device->GetTimestampFrequency() / 1000.0;

		U64 frameBeginTime = blocks[0].gpuBegin->GetTimestamp();
		for (int i = 0; i < blocks.size(); i++)
		{
			auto& block = blocks[i];
			U64 timeStart = block.gpuBegin->GetTimestamp();
			U64 timeEnd = block.gpuEnd->GetTimestamp();
			block.startTime = (F32)std::abs((F64)(timeStart - frameBeginTime) / frequency);	// => ms
			block.time = (F32)std::abs((F64)(timeEnd - timeStart) / frequency);	// => ms
			
			block.gpuBegin.reset();
			block.gpuEnd.reset();
		}
		isResolved = true;
	}

	bool BlockBuffer::HasData() const
	{
		return isResolved && !blocks.empty();
	}

	void BeginFrame()
	{
		if (!Profiler::IsEnable())
			return;

		auto device = Renderer::GetDevice();
		ASSERT(device != nullptr);

		Array<RenderStats*> statsArray;
		RenderStats::Counters.GetNotNullValues(statsArray);
		for (auto stats : statsArray)
			*stats = RenderStats();

		depth = 0;
		blockBuffers[currentBuffer].frameIndex = device->GetFrameCount();

		// Try to reslove previous frames
		for (I32 i = 0; i < ARRAYSIZE(blockBuffers); i++)
			blockBuffers[i].TryResolve();

		// Reset query pool
		auto cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);
		BeginBlockGPU("GPU Frame", *cmd);
		device->Submit(cmd);
	}
	
	void EndFrame()
	{
		if (!Profiler::IsEnable())
			return;

		ASSERT(depth == 1);
		auto device = Renderer::GetDevice();
		ASSERT(device != nullptr);
		auto cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);

		// End frame block
		auto frameBlock = blockBuffers[currentBuffer].Get(0);
		Array<RenderStats*> statsArray;
		RenderStats::Counters.GetNotNullValues(statsArray);
		for (auto stats : statsArray)
			frameBlock->stats.Max(*stats);
		
		frameBlock->gpuEnd = cmd->WriteTimestamp(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		device->Submit(cmd);

		// Prepare next frame
		currentBuffer = (currentBuffer + 1) % PROFILER_GPU_EVENTS_FRAMES;
		blockBuffers[currentBuffer].Clear();
	}

	I32 BeginBlockGPU(const char* name, GPU::CommandList& cmd, VkPipelineStageFlagBits stage)
	{
		if (!Profiler::IsEnable())
			return -1;

		ScopedMutex lock(mutex);
		Block block;
		block.name = name;
		block.depth = depth++;
		block.cmd = &cmd;
		block.stage = stage;
		block.stats = RenderStats::GetCounter();
		block.gpuBegin = cmd.WriteTimestamp(stage);

		auto& buffer = blockBuffers[currentBuffer];
		return buffer.Add(block);
	}

	void EndBlockGPU(I32 id)
	{
		if (!Profiler::IsEnable())
			return;

		ScopedMutex lock(mutex);
		auto& buffer = blockBuffers[currentBuffer];
		auto block = buffer.Get(id);
		block->stats.Mix(RenderStats::GetCounter());
		block->gpuEnd = block->cmd->WriteTimestamp(block->stage);

		depth--;
	}

	I32 BeginBlockRenderPass(const char* name, GPU::CommandList& cmd, VkPipelineStageFlagBits stage)
	{
		if (!Profiler::IsEnable())
			return -1;

		ScopedMutex lock(mutex);
		Block block;
		block.name = name;
		block.depth = depth;
		block.cmd = &cmd;
		block.stage = stage;
		block.stats = RenderStats::GetCounter();
		block.gpuBegin = cmd.WriteTimestamp(stage);

		auto& buffer = blockBuffers[currentBuffer];
		return buffer.Add(block);
	}

	void EndBlockRenderPass(I32 id)
	{
		if (!Profiler::IsEnable())
			return;

		ScopedMutex lock(mutex);
		auto& buffer = blockBuffers[currentBuffer];
		auto block = buffer.Get(id);
		block->stats.Mix(RenderStats::GetCounter());
		block->gpuEnd = block->cmd->WriteTimestamp(block->stage);
	}

	Span<BlockBuffer> GetBlockBuffers()
	{
		return Span<BlockBuffer>(blockBuffers);
	}

	bool GetLastFrameData(F32& drawTimeMs)
	{
		U64 maxFrame = 0;
		I32 maxFrameIndex = -1;
		for (I32 i = 0; i < ARRAYSIZE(blockBuffers); i++)
		{
			auto& buffer = blockBuffers[i];
			if (buffer.HasData() && buffer.frameIndex > maxFrame)
			{
				maxFrame = buffer.frameIndex;
				maxFrameIndex = i;
			}
		}

		if (maxFrameIndex != -1)
		{
			auto& buffer = blockBuffers[maxFrameIndex];
			const auto root = buffer.Get(0);
			drawTimeMs = root->time;
			return true;
		}

		drawTimeMs = 0.0f;
		return false;
	}
}
}