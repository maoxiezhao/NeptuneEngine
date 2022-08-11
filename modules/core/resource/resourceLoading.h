#pragma once

#include "core\common.h"
#include "core\platform\sync.h"
#include "core\platform\atomic.h"
#include "core\utils\task.h"

namespace VulkanTest
{
	class ContentLoadingTask : public Task
	{
	public:
		enum Type
		{
			None,
			LoadResource,
			LoadResourceData
		};

		ContentLoadingTask(Type type_) :
			type(type_)
		{
		}

		Type GetType()const {
			return type;
		}

		void Enqueue()override;

	private:
		Type type;
	};

	class ContentLoadingThread : public Thread
	{
	public:
		ContentLoadingThread();
		~ContentLoadingThread();

		void NotifyFinish();

		FORCE_INLINE bool HasFinishedSet()
		{
			return AtomicRead(&isFinished) == 0;
		}

		I32 Task() override;

	private:
		volatile I64 isFinished;
	};

	namespace ContentLoadingManager
	{
		void Initialize();
		void Uninitialize();
	}
}