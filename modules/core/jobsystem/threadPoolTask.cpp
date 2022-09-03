#include "threadPoolTask.h"
#include "core\engine.h"
#include "core\jobsystem\taskQueue.h"
#include "core\platform\platform.h"
#include "core\platform\atomic.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
	namespace
	{
		struct ThreadPoolImpl
		{
			volatile I64 ExitFlag = 0;
			ConcurrentTaskQueue<ThreadPoolTask> tasks;
			Mutex mutex;
			ConditionVariable cv;
			Array<Thread*> threads;
		};
		ThreadPoolImpl gImpl;
	}

	class ThreadPoolWorker : public Thread
	{
	public:
		I32 Task() override
		{
			return ThreadPool::ThreadProc();
		}
	};

	class ThreadPoolServiceImpl : public EngineService
	{
	public:
		ThreadPoolServiceImpl() :
			EngineService("ThreadPool", -900)
		{}

		bool Init(Engine& engine) override
		{
			I32 count = std::clamp((I32)(Platform::GetNumPhysicalCores() - 1), 1, 12);
			Logger::Info("Create thread pool worker %d", count);

			StaticString<32> name;
			gImpl.threads.reserve(count);
			for (I32 i = 0; i < count; i++)
			{
				ThreadPoolWorker* thread = CJING_NEW(ThreadPoolWorker);
				if (!thread->Create(name.Sprintf("Thread pool worker %d", i).c_str()))
				{
					CJING_SAFE_DELETE(thread);
					return false;
				}
				gImpl.threads.push_back(thread);
			}

			initialized = true;
			return true;
		}

		void Uninit() override
		{
			// All loading threads notify exit
			AtomicExchange(&gImpl.ExitFlag, 1);
			gImpl.cv.WakupAll();

			for (auto thread : gImpl.threads)
				thread->Join();

			for (auto thread : gImpl.threads)
			{
				thread->Destroy();
				CJING_SAFE_DELETE(thread);
			}

			gImpl.threads.clear();
			initialized = false;
		}
	};
	ThreadPoolServiceImpl ThreadPoolServiceImplInstance;

	void ThreadPoolTask::Enqueue()
	{
		gImpl.tasks.Add(this);
		gImpl.cv.WakupAll();
	}

	I32 ThreadPool::ThreadProc()
	{
		ThreadPoolTask* task;
		while (AtomicRead(&gImpl.ExitFlag) == 0)
		{
			if (gImpl.tasks.try_dequeue(task))
			{
				task->Execute();
			}
			else
			{
				gImpl.mutex.Lock();
				gImpl.cv.Sleep(gImpl.mutex);
				gImpl.mutex.Unlock();
			}
		}
		return 0;
	}
}