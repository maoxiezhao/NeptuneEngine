#include "resource.h"
#include "resourceManager.h"
#include "compress\compressor.h"
#include "core\engine.h"
#include "core\utils\string.h"
#include "core\profiler\profiler.h"
#include "core\threading\taskQueue.h"

namespace VulkanTest
{
	const ResourceType ResourceType::INVALID_TYPE("");

	class LoadResourceTask : public ContentLoadingTask
	{
	public:
		LoadResourceTask(Resource* resource_) :
			ContentLoadingTask(ContentLoadingTask::LoadResource),
			resource(resource_)
		{
		}

		bool Run() override
		{
			PROFILE_FUNCTION();
			ResPtr<Resource> res = resource.get();
			if (res == nullptr)
				return false;

			return resource->LoadingFromTask(this);
		}

		void OnEnd() override
		{ 
			// Loading task end, refresh resource state
			if (resource)
				resource->OnLoadEndFromTask(this);

			ContentLoadingTask::OnEnd();
		}

	private:
		WeakResPtr<Resource> resource;
	};

	ResourceType::ResourceType(const char* typeName)
	{
		ASSERT(typeName[0] == 0 || (typeName[0] >= 'a' && typeName[0] <= 'z') || (typeName[0] >= 'A' && typeName[0] <= 'Z'));
		type = StringID(typeName);
	}

	Resource::~Resource() = default;

	namespace ContentLoadingManagerImpl
	{
		extern ConcurrentTaskQueue<ContentLoadingTask> taskQueue;
	}

	bool Resource::WaitForLoaded(F32 seconds) 
	{
		// WaitForLoaded cannot be just a simple active-wait loop.
		// Because WaitForLoaded may be called on the loadingThread, active-wait loop
		// will cause the loadingThread sleep.But we don't have too many loading threads
		// Ex. Res1::Load()
		//         ResChild1:WaitForLoaded()
		//         ResChild2:WaitForLoaded()
		// 
		// In order to solve the above situation, 
		// WaitForLoaded could detect if is called from the Loading Thead and manually load dependent asset

		// Return true if resource has been already loaded
		if (IsLoaded())
		{
			if (IsInMainThread())
				GetResourceManager().TryCallOnResourceLoaded(this);
			return true;
		}

		Platform::Barrier();
		const auto loadingTask_ = loadingTask;
		if (loadingTask_ == nullptr)
		{
			Logger::Warning("Failed to waitForLoaded %s. No loadind task.", GetPath().c_str());
			return false;
		}

		PROFILE_FUNCTION();
		auto loadingThread = ContentLoadingManager::GetCurrentLoadThread();
		if (loadingThread == nullptr)
		{
			// Is not called from contentloadingThread, just wait it
			loadingTask_->Wait(seconds);
		}
		else
		{
			Task* task = loadingTask;
			Array<ContentLoadingTask*> localQueue;
			while (!Engine::ShouldExit())
			{
				// Try to execute content tasks
				while (task->IsQueued() && !Engine::ShouldExit())
				{
					// Dequeue task from the loading queue
					ContentLoadingTask* tmp;
					if (ContentLoadingManagerImpl::taskQueue.try_dequeue(tmp))
					{
						if (tmp == task)
						{
							// Put back queued tasks
							if (localQueue.size() != 0)
							{
								ContentLoadingManagerImpl::taskQueue.enqueue_bulk(localQueue.data(), localQueue.size());
								localQueue.clear();
							}

							// Run it manually
							loadingThread->Run(tmp);
						}
						else
						{
							// Pending all other loading tasks
							localQueue.push_back(tmp);
						}
					}
					else
					{
						break;
					}
				}

				// Put back queued tasks
				if (localQueue.size() != 0)
				{
					ContentLoadingManagerImpl::taskQueue.enqueue_bulk(localQueue.data(), localQueue.size());
					localQueue.clear();
				}

				// Check if task is ended
				if (task->IsEnded())
				{
					if (!task->IsFinished())
						break;

					// If was fine then wait for the next task
					task = task->GetNextTask();
					if (!task)
						break;
				}
			}
		}


		if (IsInMainThread() && IsLoaded())
			GetResourceManager().TryCallOnResourceLoaded(this);

		return isLoaded;
	}

	void Resource::Refresh()
	{
		if (currentState == State::EMPTY)
			return;

		State oldState = currentState;
		currentState = State::EMPTY;
		StateChangedCallback.Invoke(oldState, currentState, *this);
		CheckState();
	}

	void Resource::AddDependency(Resource& depRes)
	{
		ASSERT(desiredState != State::EMPTY);

		depRes.StateChangedCallback.Bind<&Resource::OnStateChanged>(this);
		if (depRes.IsEmpty())
			emptyDepCount++;
		if (depRes.IsFailure())
			failedDepCount++;

		CheckState();
	}

	void Resource::RemoveDependency(Resource& depRes)
	{
		depRes.StateChangedCallback.Unbind<&Resource::OnStateChanged>(this);
		if (depRes.IsEmpty())
		{
			ASSERT(emptyDepCount > 1 || (emptyDepCount == 1));
			emptyDepCount--;
		}
		if (depRes.IsFailure())
		{
			ASSERT(failedDepCount > 0);
			failedDepCount--;
		}

		CheckState();
	}

	void Resource::SetIsTemporary()
	{
		isTemporary = true;
		isLoaded = true;
	}

	I32 Resource::GetReference() const
	{
		return (I32)AtomicRead(const_cast<I64 volatile*>(&refCount));
	}

	void Resource::OnDelete()
	{
		ASSERT(IsInMainThread());

		const bool wasMarkedToDelete = toDelete != 0;

		// Fire unload event in main thread
		OnUnLoadedMainThread();

		// Remove it from resources pool
		GetResourceManager().OnResourceUnload(this);

		// Unload real resource content
		mutex.Lock();
		desiredState = State::EMPTY;
		if (IsLoaded())
		{
			Unload();
			isLoaded = false;
		}
		// Refresh current state
		resSize = 0;
		emptyDepCount = 1;
		failedDepCount = 0;
		CheckState();
		mutex.Unlock();

#if CJING3D_EDITOR
		ResourceManager* resManager = &GetResourceManager();
		if (wasMarkedToDelete)
			resManager->DeleteResource(GetPath());
#endif

		// Delete self
		Object::OnDelete();
	}

	ResourceManager& Resource::GetResourceManager()
	{
		return resManager;
	}

	Resource::Resource(const ResourceInfo& info, ResourceManager& resManager_) :
		ScriptingObject(ScriptingObjectParams(info.guid)),
		emptyDepCount(1),
		failedDepCount(0),
		currentState(State::EMPTY),
		desiredState(State::EMPTY),
		resSize(0),
		path(info.path),
		loadingTask(nullptr),
		refCount(0),
		resManager(resManager_)
	{
	}

	void Resource::DoLoad()
	{
		if (IsLoaded())
			return;

		desiredState = State::READY;

		ASSERT(loadingTask == nullptr);
		loadingTask = CreateLoadingTask();
		ASSERT(loadingTask != nullptr);
		loadingTask->Start();
	}

	void Resource::Reload()
	{
		ASSERT(IsInMainThread());

		// Temporary is memory-only
		if (IsTemporary())
			return;

		Logger::Info("Reloading resource %s", GetPath().c_str());

		// Wait for resource loaded
		WaitForLoaded();

		// Fire event
		OnReloadingCallback.Invoke(this);

		ScopedMutex lock(mutex);

		// Unload resource content
		desiredState = State::EMPTY;
		if (IsLoaded())
		{
			Unload();
			isLoaded = false;
		}

		// Refresh current state
		resSize = 0;
		emptyDepCount = 1;
		failedDepCount = 0;
		isStateDirty = true;

		// Start reloading
		DoLoad();

		isReloading = false;
	}

	void Resource::CheckState()
	{
		// Refresh the current state according to failedDep and emtpyDep
		isStateDirty = false;
		State oldState = currentState;
		if (currentState != State::FAILURE && failedDepCount > 0)
		{
			currentState = State::FAILURE;
			StateChangedCallback.Invoke(oldState, currentState, *this);
		}

		if (failedDepCount == 0)
		{
			if (currentState != State::READY && desiredState != State::EMPTY && emptyDepCount == 0)
			{
				currentState = State::READY;
				StateChangedCallback.Invoke(oldState, currentState, *this);
			}

			if (currentState != State::EMPTY && emptyDepCount > 0)
			{
				currentState = State::EMPTY;
				StateChangedCallback.Invoke(oldState, currentState,*this);
			}
		}
	}

	void Resource::OnCreated(State state)
	{
		ASSERT(emptyDepCount == 1);
		ASSERT(failedDepCount == 0);

		currentState = state;
		desiredState = State::READY;
		failedDepCount = state == State::FAILURE ? 1 : 0;
		emptyDepCount = 0;
	}

	void Resource::CancelStreaming()
	{
	}

	ContentLoadingTask* Resource::CreateLoadingTask()
	{
		LoadResourceTask* task = CJING_NEW(LoadResourceTask)(this);
		return task;
	}

	bool Resource::LoadingFromTask(LoadResourceTask* task)
	{
		if (loadingTask == nullptr)
			return false;

		mutex.Lock();
		bool isLoaded_ = LoadResource();
		loadingTask = nullptr;
		isLoaded = isLoaded_;
		mutex.Unlock();

		// Fire onLoaded event
		if (isLoaded)
			GetResourceManager().OnResourceLoaded(this);

		return isLoaded;
	}

	void Resource::OnLoadEndFromTask(LoadResourceTask* task)
	{
		ScopedMutex lock(mutex);

		// Refresh resoruce state
		const auto curState = currentState;
		ASSERT(curState != State::READY);
		auto state = task->GetState();
		isLoaded = state == Task::State::Finished;

		if (state == Task::State::Finished)
		{
			emptyDepCount--;
		}
		else if (state == Task::State::Failed)
		{
			emptyDepCount--;
			failedDepCount++;
		}
		else if (state == Task::State::Canceled)
		{
			desiredState = State::EMPTY;
		}
		isStateDirty = true;
	}

	void Resource::OnLoadedMainThread()
	{
		OnLoadedCallback.Invoke(this);
	}

	void Resource::OnUnLoadedMainThread()
	{
		OnUnloadedCallback.Invoke(this);

		if (loadingTask)
		{
			auto task = loadingTask;
			loadingTask = nullptr;
			task->Cancel();
		}
	}

	void Resource::OnStateChanged(State oldState, State newState, Resource& res)
	{
		ASSERT(oldState != newState);
		ASSERT(currentState != State::EMPTY || desiredState != State::EMPTY);

		if (oldState == State::EMPTY)
		{
			ASSERT(emptyDepCount > 0);
			emptyDepCount--;
		}
		if (oldState == State::FAILURE)
		{
			ASSERT(failedDepCount > 0);
			failedDepCount--;
		}

		if (newState == State::EMPTY)
			emptyDepCount++;
		if (newState == State::FAILURE)
			failedDepCount++;

		CheckState();
	}
}