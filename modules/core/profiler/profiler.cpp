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
		memset(block, 0, sizeof(Block));
		block->id = index;
		block->startTime = time;
		block->endTime = 0;
		block->depth = depth++;
		return block;
	}

	void Thread::EndBlock(I32 index)
	{
		const F64 time = Timer::GetTimeSeconds();
		Block* block = entBuffer.Get(index);
		block->endTime = time;
		depth--;
	}

	Block* Thread::BeginFiberWait()
	{
		const F64 time = Timer::GetTimeSeconds();
		const I32 index = entBuffer.Add();
		Block* block = entBuffer.Get(index);
		memset(block, 0, sizeof(Block));
		block->id = index;
		block->startTime = time;
		block->endTime = 0;
		block->depth = depth - 1;
		return block;
	}

	void Thread::EndFiberWait(I32 index)
	{
		const F64 time = Timer::GetTimeSeconds();
		Block* block = entBuffer.Get(index);
		block->endTime = time;
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
		for (auto thread : gImpl.contexts)
			ASSERT(thread->blockStack.size() < 256);
	}

	void BeginBlock(const char* name)
	{
		if (!gImpl.enabled)
			return;

		Thread* thread = gImpl.GetThreadLocalContext();
		Block* block = thread->BeginBlock();
		block->name = name;
		block->type = BlockType::CPU_BLOCK;

		thread->blockStack.push_back(block->id);
		CurrentThread = thread;
	}

	void EndBlock()
	{
		if (!gImpl.enabled ||
			CurrentThread == nullptr || 
			CurrentThread->blockStack.empty())
			return;

		I32 index = CurrentThread->blockStack.back();
		CurrentThread->blockStack.pop_back();
		CurrentThread->EndBlock(index);
	}

	FiberSwitchData BeginFiberWait()
	{
		if (!gImpl.enabled)
			return FiberSwitchData();

		Thread* thread = gImpl.GetThreadLocalContext();
		Block* block = thread->BeginFiberWait();
		block->name = "WaitJob";
		block->type = BlockType::FIBER;

		FiberSwitchData switchData;
		switchData.id = block->id;
		switchData.count = thread->blockStack.size();
		switchData.thread = thread;
		ASSERT(switchData.count < (U32)ARRAYSIZE(switchData.blocks));
		memcpy(switchData.blocks, thread->blockStack.data(), std::min(switchData.count, (U32)ARRAYSIZE(switchData.blocks)) * sizeof(I32));
		
		CurrentThread = thread;
		return switchData;
	}

	void ContinueBlock(I32 index, Thread* prevThread)
	{
		ASSERT(CurrentThread != nullptr);
		ASSERT(prevThread != nullptr);

		Block* prevBlock = prevThread->entBuffer.Get(index);
		ASSERT(prevBlock != nullptr);

		Block* continueBlock = CurrentThread->BeginBlock();
		continueBlock->type = prevBlock->type;
		continueBlock->name = prevBlock->name;
		CurrentThread->blockStack.push_back(continueBlock->id);
	}

	void EndFiberWait(const FiberSwitchData& switchData)
	{
		// Notice:
		// If the job waiting before dose not set the target worker
		// there will be a rand worker to continue the job, the thread will be changed

		if (!gImpl.enabled || switchData.thread == nullptr || CurrentThread == nullptr)
			return;

		switchData.thread->EndFiberWait(switchData.id);

		for (I32 i = 0; i < switchData.count; i++)
		{
			if (i < ARRAYSIZE(switchData.blocks))
				ContinueBlock(switchData.blocks[i], switchData.thread);
			else
				ContinueBlock(-1, switchData.thread);
		}
	}

	void BeforeFiberSwitch()
	{
		Thread* thread = gImpl.GetThreadLocalContext();
		while (!thread->blockStack.empty())
		{
			thread->EndBlock(thread->blockStack.back());
			thread->blockStack.pop_back();
		}
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


