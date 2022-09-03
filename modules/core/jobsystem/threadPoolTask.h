#pragma once

#include "core\common.h"
#include "core\jobsystem\task.h"

namespace VulkanTest
{
	class ThreadPoolTask : public Task
	{
	public:
		void Enqueue()override;
	};

	class ThreadPool
	{
		friend class ThreadPoolWorker;
		friend class ThreadPoolService;

	private:
		static I32 ThreadProc();
	};
}