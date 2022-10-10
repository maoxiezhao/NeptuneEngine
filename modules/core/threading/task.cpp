#include "task.h"
#include "core\platform\platform.h"
#include "core\platform\timer.h"

namespace VulkanTest
{
	Task* Task::StartNew(Task* task)
	{
		task->Start();
		return task;
	}

	void Task::Start()
	{
		if (state != State::Created)
			return;

		state = State::Queued;
		Enqueue();
	}

	void Task::Execute()
	{
		if (IsCanceled())
			return;

		ASSERT(IsQueued());
		state = State::Running;

		bool ret = Run();

		if (CancelRequested())
			state = State::Canceled;
		else if (ret == false)
			OnFail();
		else
			OnFinish();
	}

	void Task::Cancel()
	{
		if (AtomicRead(&cancelFlag) == 0)
		{
			OnCancel();

			if (nextTask)
				nextTask->Cancel();
		}
	}

	void Task::OnFail()
	{
		state = State::Failed;

		if (nextTask)
			nextTask->OnFail();
	
		OnEnd();
	}

	void Task::OnEnd()
	{
		ASSERT(!IsRunning());
		DeleteObject(10.0f);
	}

	bool Task::Wait(F32 seconds) const
	{
		Timer timer;
		do
		{
			auto state = GetState();
			if (state == State::Finished)
			{
				// Wait for the next task with the remaining time
				if (nextTask)
					return nextTask->Wait(seconds - timer.GetTimeSinceStart());
				return true;
			}

			if (state == State::Failed || state == State::Canceled)
				return false;

			Platform::Sleep(1.0f);
		} 
		while (seconds <= 0.0f || timer.GetTimeSinceStart() < seconds);

		Logger::Warning("Task has timed out %f", timer.GetTimeSinceStart());
		return false;
	}

	void Task::SetNextTask(Task* task)
	{
		ASSERT(task && task != this);
		if (nextTask)
			return nextTask->SetNextTask(task);

		nextTask = task;
	}

	void Task::OnFinish()
	{
		ASSERT(IsRunning());
		ASSERT(!CancelRequested());
		state = State::Finished;

		if (nextTask)
			nextTask->Start();

		OnEnd();
	}

	void Task::OnCancel()
	{
		AtomicIncrement(&cancelFlag);
		Platform::Barrier();

		if (IsRunning())
		{
			// Task is running, wait for a while
			Wait(2000.0f);
		}

		const State curState = GetState();
		if (curState != State::Finished && curState != State::Failed)
		{
			state = State::Canceled;
			OnEnd();
		}
	}
}