#include "profiler.h"
#include "platform\platform.h"
#include "platform\sync.h"
#include "string.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
	struct ThreadLocalContext
	{
		StaticString<64> name;
		Mutex mutex;
		U32 threadID = 0;
	};

	struct ProfilerImpl
	{
		Mutex mutex;
		std::vector<ThreadLocalContext*> contexts;
		DefaultAllocator allocator;

		ProfilerImpl()
		{
		}

		~ProfilerImpl()
		{
		}

		ThreadLocalContext* GetThreadLocalContext()
		{
			thread_local ThreadLocalContext* ctx = [&]() 
			{
				void* mem = allocator.Allocate(sizeof(ThreadLocalContext));
				ThreadLocalContext* newCtx = new(mem) ThreadLocalContext();
				newCtx->threadID = Platform::GetCurrentThreadID();
				ScopedMutex lock(mutex);
				contexts.push_back(newCtx);
				return newCtx;
			}();
			return ctx;
		}
	};
	ProfilerImpl gImpl;

	void Profiler::SetThreadName(const char* name)
	{
		ThreadLocalContext* ctx = gImpl.GetThreadLocalContext();
		ctx->name = name;
	}
}


