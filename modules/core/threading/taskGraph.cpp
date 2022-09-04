#include "taskGraph.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
	void TaskGraphSystem::AddDependency(TaskGraphSystem* system)
	{
		dependencies.push_back(system);
	}

	TaskGraph::TaskGraph() = default;
	TaskGraph::~TaskGraph() = default;

	void TaskGraph::AddSystem(TaskGraphSystem* system)
	{
		ASSERT(system != nullptr);
		systems.push_back(system);
	}

	void TaskGraph::RemoveSystem(TaskGraphSystem* system)
	{
		ASSERT(system != nullptr);
		systems.erase(system);
	}

	void TaskGraph::Execute()
	{
		PROFILE_FUNCTION();

		queue.clear();
		remaining.clear();
		
		for (auto system : systems)
			remaining.push_back(system);

		while (!remaining.empty())
		{
			for (I32 i = (I32)remaining.size() - 1; i >= 0; i--)
			{
				auto system = remaining[i];
				bool hasDepReady = true;
				for (auto d : system->dependencies)
				{
					if (remaining.indexOf(d) >= 0)
					{
						hasDepReady = false;
						break;
					}
				}

				// All dependencies are ready
				if (hasDepReady)
				{
					queue.push_back(system);
					remaining.eraseAt(i);
				}
			}

			if (queue.empty())
				break;

			Jobsystem::JobHandle jobHandle;
			for (auto system : queue)
				system->Execute(&jobHandle);
			queue.clear();

			// Wait all jobs
			Jobsystem::Wait(&jobHandle);
		}
	}
}