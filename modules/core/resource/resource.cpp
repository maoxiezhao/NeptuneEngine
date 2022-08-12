#include "resource.h"
#include "resourceManager.h"
#include "core\utils\string.h"
#include "core\compress\compressor.h"
#include "core\utils\profiler.h"

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
			// Loading task finished, refresh resource state
			if (resource)
				resource->OnLoadedFromTask(this);

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

	I32 Resource::GetReference() const
	{
		return (I32)AtomicRead(const_cast<I64 volatile*>(&refCount));
	}

	ResourceFactory& Resource::GetResourceFactory()
	{
		return resFactory;
	}

	ResourceManager& Resource::GetResourceManager()
	{
		return resFactory.GetResourceManager();
	}

	Resource::Resource(const Path& path_, ResourceFactory& resFactory_) :
		emptyDepCount(1),
		failedDepCount(0),
		currentState(State::EMPTY),
		desiredState(State::EMPTY),
		resSize(0),
		path(path_),
		resFactory(resFactory_),
		loadingTask(nullptr),
		refCount(0)
	{
	}

	void Resource::DoLoad()
	{
		if (IsReady())
			return;

		desiredState = State::READY;

		ASSERT(loadingTask == nullptr);
		loadingTask = CreateLoadingTask();
		ASSERT(loadingTask != nullptr);
		loadingTask->Start();
	}

	void Resource::DoUnload()
	{
		desiredState = State::EMPTY;
		OnUnLoaded();
		
		// Unload real resource content
		mutex.Lock();
		if (IsReady())
			Unload();
		mutex.Unlock();

		// Refresh current state
		resSize = 0;
		emptyDepCount = 1;
		failedDepCount = 0;
		CheckState();
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

	ContentLoadingTask* Resource::CreateLoadingTask()
	{
		LoadResourceTask* task = CJING_NEW(LoadResourceTask)(this);
		return task;
	}

	bool Resource::LoadingFromTask(LoadResourceTask* task)
	{
		if (loadingTask == nullptr)
			return false;

		ScopedMutex lock(mutex);
		bool ret = LoadResource();
		loadingTask = nullptr;
		return ret;
	}

	void Resource::OnLoadedFromTask(LoadResourceTask* task)
	{
		ScopedMutex lock(mutex);

		// Refresh resoruce state
		ASSERT(currentState != State::READY);
		ASSERT(emptyDepCount == 1);

		auto state = task->GetState();
		if (state == Task::State::Finished)
		{
			emptyDepCount--;
			GetResourceFactory().OnResourceLoaded(this);
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

	void Resource::OnLoaded()
	{
		OnLoadedCallback.Invoke(this);
	}

	void Resource::OnUnLoaded()
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