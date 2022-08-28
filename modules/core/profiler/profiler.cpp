#include "profiler.h"
#include "platform\timer.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
namespace Profiler
{
	BlockBuffer::BlockBuffer()
	{
		capacity = 8192;
		mask = capacity - 1;
		data = static_cast<Block*>(malloc(sizeof(Block) * capacity));
		ASSERT(data != nullptr);
		memset(data, 0, sizeof(sizeof(Block) * capacity));
		head = 0;
		count = 0;
	}
	
	BlockBuffer::~BlockBuffer()
	{
		free(data);
	}

	void BlockBuffer::GetBlocks(std::vector<Block>& blocks)
	{
		blocks.clear();

		if (count == 0)
			return;

		// Find first block
		Iterator firstEvent = Begin();
		for (auto i = firstEvent; i.IsNotEnd(); ++i)
		{
			if (i.Block().depth == 0)
			{
				firstEvent = i;
				break;
			}
		}

		if (firstEvent.IsEnd())
			return;

		// Find the last item (last event in ended root event)
		Iterator lastEndedRoot = End();
		for (auto i = Last(); i != firstEvent; --i)
		{
			if (i.Block().depth == 0 && i.Block().endTime != 0)
			{
				lastEndedRoot = i;
				break;
			}
		}

		if (lastEndedRoot.IsEnd())
			return;


		// Find the last non-root event in last root event
		Iterator lastEvent = lastEndedRoot;
		const double lastRootEventEndTime = lastEndedRoot.Block().endTime;
		for (auto i = --End(); i != lastEndedRoot; --i)
		{
			if (i.Block().endTime != 0 && i.Block().endTime <= lastRootEventEndTime)
			{
				lastEvent = i;
				break;
			}
		}

		// Remove all the events between [Begin(), lastEvent]
		count -= (lastEvent.Index() - Begin().Index()) & mask;
	
		const I32 head = (lastEvent.Index() + 1) & mask;
		count = (lastEvent.Index() - firstEvent.Index() + 1) & mask;
		blocks.resize(count);

		I32 tail = (head - count) & mask;
		I32 spaceLeft = capacity - tail;
		I32 spaceLeftCount = std::min(spaceLeft, count);
		I32 overflow = count - spaceLeft;


		// [ ----------tail ----------]
		//                 |leftCount|
		memcpy(blocks.data(), &data[tail], spaceLeftCount * sizeof(Block));
		if (overflow > 0)
		{
			// [ ----------tail ----------]
			//  |Overflow|
			memcpy(blocks.data() + spaceLeftCount, &data[0], overflow * sizeof(Block));
		}
	}

	Block* Thread::BeginBlock()
	{
		const F64 time = Timer::GetTimeSeconds();
		const I32 index = entBuffer.Add();
		Block* block = entBuffer.Get(index);
		block->id = index;
		block->startTime = time;
		block->endTime = 0;
		block->depth = depth++;

		if (depth == 0)
			int a = 0;
		return block;
	}

	void Thread::EndBlock(I32 index)
	{
		const F64 time = Timer::GetTimeSeconds();
		Block* block = entBuffer.Get(index);
		block->endTime = time;
		depth--;
	}

	void Thread::GetBlocks(std::vector<Block>& blocks)
	{
		return entBuffer.GetBlocks(blocks);
	}

	thread_local Thread* CurrentThread = nullptr;

	struct ProfilerImpl
	{
		Mutex mutex;
		std::vector<Thread*> contexts;
		DefaultAllocator allocator;
		bool enabled = false;

		ProfilerImpl()
		{
		}

		~ProfilerImpl()
		{
			for (auto thread : contexts)
				thread->~Thread();
			contexts.clear();
		}

		Thread* GetThreadLocalContext()
		{
			thread_local Thread* ctx = [&]() 
			{
				void* mem = allocator.Allocate(sizeof(Thread));
				Thread* newCtx = new(mem) Thread();
				newCtx->threadID = Platform::GetCurrentThreadID();
				ScopedMutex lock(mutex);
				contexts.push_back(newCtx);
				return newCtx;
			}();
			return ctx;
		}
	};
	ProfilerImpl gImpl;

	void SetThreadName(const char* name)
	{
		Thread* ctx = gImpl.GetThreadLocalContext();
		ctx->name = name;
	}

	void BeginFrame()
	{
		for (auto thread : gImpl.contexts)
			thread->entBuffer.Clear();
	}

	void EndFrame()
	{
	}

	I32 BeginBlock(const char* name)
	{
		if (!gImpl.enabled)
			return -1;

		Thread* thread = gImpl.GetThreadLocalContext();
		Block* block = thread->BeginBlock();
		block->name = name;
		block->type = BlockType::CPU_BLOCK;

		CurrentThread = thread;
		return block->id;
	}

	void EndBlock(I32 index)
	{
		if (!gImpl.enabled ||
			CurrentThread == nullptr ||
			index < 0)
			return;

		CurrentThread->EndBlock(index);
	}

	void Enable(bool enabled)
	{
		gImpl.enabled = enabled;
	}

	std::vector<Thread*>& GetThreads()
	{
		return gImpl.contexts;
	}
}
}


