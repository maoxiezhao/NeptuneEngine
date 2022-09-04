#pragma once

#include "core\common.h"
#include "core\threading\task.h"

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