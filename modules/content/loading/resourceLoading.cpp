#include "resourceLoading.h"
#include "core\threading\taskQueue.h"
#include "core\platform\platform.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
	namespace ContentLoadingManagerImpl
	{
		thread_local ContentLoadingThread* ThisThread = nullptr;
		Mutex mutex;
		ConditionVariable cv;
		ConcurrentTaskQueue<ContentLoadingTask> taskQueue;
		std::vector<ContentLoadingThread*> threads;
		ContentLoadingThread* MainThread = nullptr;
	}
	using namespace ContentLoadingManagerImpl;

	void ContentLoadingTask::Enqueue()
	{
		taskQueue.Add(this);
		cv.Wakeup();
	}

	ContentLoadingThread::ContentLoadingThread() :
		isFinished(0)
	{
	}

	ContentLoadingThread::~ContentLoadingThread()
	{
	}

	void ContentLoadingThread::NotifyFinish()
	{
		AtomicIncrement(&isFinished);
	}

	I32 ContentLoadingThread::Task()
	{
		Profiler::SetThreadName("ContentLoading");
		ContentLoadingTask* task = nullptr;
		ThisThread = this;
		while (HasFinishedSet())
		{
			if (taskQueue.try_dequeue(task))
			{
				Run(task);
			}
			else
			{
				mutex.Lock();
				cv.Sleep(mutex);
				mutex.Unlock();
			}
		}
		ThisThread = nullptr;
		return 0;
	}

	void ContentLoadingThread::Run(ContentLoadingTask* task)
	{
		ASSERT(task != nullptr);
		task->Execute();
	}

	void ContentLoadingManager::Initialize()
	{
		I32 count = std::clamp((I32)(Platform::GetCPUsCount() * 0.2f), 1, 12);
		Logger::Info("Create content loading threads %d", count);

		MainThread = CJING_NEW(ContentLoadingThread);
		ThisThread = MainThread;

		StaticString<32> name;
		threads.reserve(count);
		for (I32 i = 0; i < count; i++)
		{
			ContentLoadingThread* thread = CJING_NEW(ContentLoadingThread);
			if (!thread->Create(name.Sprintf("Loading thread %d", i).c_str()))
			{
				CJING_SAFE_DELETE(thread);
				return;
			}
			threads.push_back(thread);
		}
	}

	void ContentLoadingManager::Uninitialize()
	{
		// All loading threads notify exit
		for (auto thread : threads)
			thread->NotifyFinish();

		cv.WakupAll();

		for (auto thread : threads)
			thread->Join();

		for (auto thread : threads)
		{
			thread->Destroy();
			CJING_SAFE_DELETE(thread);
		}

		threads.clear();

		CJING_SAFE_DELETE(MainThread);
		MainThread = nullptr;
		ThisThread = nullptr;

		// Cancel all loading tasks
		taskQueue.CancelAll();
	}

	ContentLoadingThread* ContentLoadingManager::GetCurrentLoadThread()
	{
		return ThisThread;
	}
}