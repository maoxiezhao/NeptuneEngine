#pragma once

#include "core\common.h"
#include "core\memory\object.h"
#include "core\collections\array.h"
#include "jobsystem.h"

namespace VulkanTest
{
	class TaskGraph;

	// TaskGraphSystem is used for asynchronous execution.
	class VULKAN_TEST_API TaskGraphSystem : public Object
	{
	public:
		void AddDependency(TaskGraphSystem* system);
		virtual void Execute(Jobsystem::JobHandle* handle) = 0;

	private:
		friend class TaskGraph;

		Array<TaskGraphSystem*> dependencies;
	};

	class VULKAN_TEST_API TaskGraph : public Object
	{
	public:
		TaskGraph();
		~TaskGraph();

		void AddSystem(TaskGraphSystem* system);
		void RemoveSystem(TaskGraphSystem* system);
		void Execute();

	private:
		Array<TaskGraphSystem*> systems;
		Array<TaskGraphSystem*> remaining;
		Array<TaskGraphSystem*> queue;
	};
}