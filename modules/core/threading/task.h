#pragma once

#include "core\common.h"
#include "core\types\object.h"
#include "core\platform\atomic.h"

namespace VulkanTest
{
	class Task : public Object
	{
	public:
		Task() = default;
		Task(const Task&) = delete;
		Task& operator=(const Task&) = delete;

		enum State
		{
			Created, 
			Failed, 
			Canceled, 
			Queued, 
			Running, 
			Finished
		};

		Task* GetNextTask() const {
			return nextTask;
		}

		State GetState() const { 
			return static_cast<State>(AtomicRead((I64 volatile*)&state));
		}

        bool IsFailed() const { return GetState() == State::Failed; }
        bool IsCanceled() const { return GetState() == State::Canceled; }
        bool IsQueued() const { return GetState() == State::Queued; }
        bool IsRunning() const { return GetState() == State::Running; }
        bool IsFinished() const { return GetState() == State::Finished; }
		bool IsEnded() const
		{
			auto state = GetState();
			return state == State::Failed || state == State::Canceled || state == State::Finished;
		}

		bool CancelRequested()const { return AtomicRead((I64*)&cancelFlag) != 0; }

		virtual void Enqueue() = 0;

		static Task* StartNew(Task* task);

	public:
		void Start();
		void Execute();
		void Cancel();
		bool Wait(F32 seconds = -1) const;
		void SetNextTask(Task* task);

	protected:
		virtual void OnFinish();
		virtual void OnCancel();
		virtual void OnFail();
		virtual void OnEnd();

		virtual bool Run() = 0;

		volatile State state = State::Created;
		volatile I64 cancelFlag = 0;
		Task* nextTask = nullptr;
	};
}