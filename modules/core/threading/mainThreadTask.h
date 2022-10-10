#pragma once

#include "core\common.h"
#include "core\threading\task.h"
#include "core\utils\delegate.h"

namespace VulkanTest
{
#define INVOKE_ON_MAIN_THREAD(targetType, targetMethod, targetObject) \
    if (IsInMainThread()) \
	{ \
		targetObject->targetMethod(); \
	} else { \
		Delegate<void()> action; \
		action.Bind<&targetMethod>(targetObject); \
		Task::StartNew(New<MainThreadActionTask>(action))->Wait(); \
	}

	class MainThreadTask : public Task
	{
	public:
		void Enqueue()override;

		// Run all pending MainThreadTask, it called by the engine in main thread
		static void RunAll(F32 dt);
	};

	class MainThreadActionTask : public MainThreadTask
	{
	public:
		MainThreadActionTask(Delegate<void()>& action_, Object* obj = nullptr) :
			MainThreadTask(),
			action(action_),
			object(obj)
		{
		}

	protected:
		bool Run() override;

	private:
		Delegate<void()> action;
		Object* object;
	};
}