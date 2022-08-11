#include "resourceLoading.h"
#include "core\collections\concurrentqueue.hpp"
#include "core\platform\platform.h"

namespace VulkanTest
{
	namespace
	{
		template<typename T>
		class ConcurrentTaskQueue : public ConcurrentQueue<T*>
		{
		public:
			FORCE_INLINE void Add(T* item)
			{
				ConcurrentQueue<T*>::enqueue(item);
			}

			void CancelAll()
			{
				T* tasks[16];
				std::size_t count;
				while ((count = ConcurrentQueue<T*>::try_dequeue_bulk(tasks, ARRAYSIZE(tasks))) != 0)
				{
					for (std::size_t i = 0; i != count; i++)
					{
						tasks[i]->Cancel();
					}
				}
			}
		};

		thread_local ContentLoadingThread* ThisThread = nullptr;
		struct ContentLoadingImpl
		{
			Mutex mutex;
			ConditionVariable cv;
			ConcurrentTaskQueue<ContentLoadingTask> taskQueue;
			Array<ContentLoadingThread*> threads;
			ContentLoadingThread* MainThread = nullptr;
		};
		ContentLoadingImpl gImpl;
	}

	void ContentLoadingTask::Enqueue()
	{
		gImpl.taskQueue.Add(this);
		gImpl.cv.Wakeup();
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
		ContentLoadingTask* task = nullptr;
		ThisThread = this;
		while (HasFinishedSet())
		{
			if (gImpl.taskQueue.try_dequeue(task))
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
		ThisThread = nullptr;
		return 0;
	}

	void ContentLoadingManager::Initialize()
	{
		I32 count = std::clamp((I32)(Platform::GetCPUsCount() * 0.2f), 1, 12);
		Logger::Info("Create content loading threads %d", count);

		gImpl.MainThread = CJING_NEW(ContentLoadingThread);
		ThisThread = gImpl.MainThread;

		StaticString<32> name;
		gImpl.threads.reserve(count);
		for (I32 i = 0; i < count; i++)
		{
			ContentLoadingThread* thread = CJING_NEW(ContentLoadingThread);
			if (!thread->Create(name.Sprintf("Loading thread %d", i).c_str()))
			{
				CJING_SAFE_DELETE(thread);
				return;
			}
			gImpl.threads.push_back(thread);
		}
	}

	void ContentLoadingManager::Uninitialize()
	{
		// All loading threads notify exit
		for (auto thread : gImpl.threads)
			thread->NotifyFinish();

		gImpl.cv.WakupAll();

		for (auto thread : gImpl.threads)
			thread->Join();

		for (auto thread : gImpl.threads)
		{
			thread->Destroy();
			CJING_SAFE_DELETE(thread);
		}

		gImpl.threads.clear();

		CJING_SAFE_DELETE(gImpl.MainThread);
		gImpl.MainThread = nullptr;
		ThisThread = nullptr;

		// Cancel all loading tasks
		gImpl.taskQueue.CancelAll();
	}
}