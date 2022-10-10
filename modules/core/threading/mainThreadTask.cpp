#include "mainThreadTask.h"
#include "core\profiler\profiler.h"
#include "core\collections\array.h"

namespace VulkanTest
{
	namespace
	{
		Mutex mutex;
		Array<MainThreadTask*> Queue;
	}

	void MainThreadTask::Enqueue()
	{
		ScopedMutex lock(mutex);
		Queue.push_back(this);
	}

	void MainThreadTask::RunAll(F32 dt)
	{
		PROFILE_FUNCTION();
		ScopedMutex lock(mutex);
		for (auto task : Queue)
			task->Execute();
		Queue.clear();
	}

	bool MainThreadActionTask::Run()
	{
		if (action.IsBinded())
			action.Invoke();
		return true;
	}
}