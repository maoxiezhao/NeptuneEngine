#include "resource.h"
#include "resourceManager.h"
#include "core\utils\string.h"
#include "core\compress\compressor.h"

namespace VulkanTest
{
	const ResourceType ResourceType::INVALID_TYPE("");

	ResourceType::ResourceType(const char* typeName)
	{
		ASSERT(typeName[0] == 0 || (typeName[0] >= 'a' && typeName[0] <= 'z') || (typeName[0] >= 'A' && typeName[0] <= 'Z'));
		type = StringID(typeName);
	}

	void ResourceDeleter::operator()(Resource* res)
	{
		if (res->resFactory.IsUnloadEnable())
			res->DoUnload();

		if (res->ownedBySelf)
			CJING_SAFE_DELETE(res);
	}

	Resource::~Resource() = default;

	void Resource::Refresh()
	{
		if (currentState == State::EMPTY)
			return;

		State oldState = currentState;
		currentState = State::EMPTY;
		cb.Invoke(oldState, currentState, *this);
		CheckState();
	}

	void Resource::AddDependency(Resource& depRes)
	{
		ASSERT(desiredState != State::EMPTY);

		depRes.cb.Bind<&Resource::OnStateChanged>(this);
		if (depRes.IsEmpty())
			emptyDepCount++;
		if (depRes.IsFailure())
			failedDepCount++;

		CheckState();
	}

	void Resource::RemoveDependency(Resource& depRes)
	{
		depRes.cb.Unbind<&Resource::OnStateChanged>(this);
		if (depRes.IsEmpty())
		{
			ASSERT(emptyDepCount > 1 || (emptyDepCount == 1)); // && !asyncHandle.IsValid()));
			emptyDepCount--;
		}
		if (depRes.IsFailure())
		{
			ASSERT(failedDepCount > 0);
			failedDepCount--;
		}

		CheckState();
	}

	void Resource::OnContentLoaded(Task::State state)
	{
		ASSERT(currentState != State::READY);
		ASSERT(emptyDepCount == 1);

		if (state == Task::State::Finished)
		{
			emptyDepCount--;
			CheckState();
		}
		else if (state == Task::State::Failed)
		{
			emptyDepCount--;
			failedDepCount++;
			CheckState();
		}
		else if (state == Task::State::Canceled)
		{
			desiredState = State::EMPTY;
			CheckState();
		}
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
		loadingTask(nullptr)
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
		ASSERT(emptyDepCount <= 1);

		// Refresh current state
		resSize = 0;
		emptyDepCount = 1;
		failedDepCount = 0;
		CheckState();
	}

	void Resource::CheckState()
	{
		// Refresh the current state according to failedDep and emtpyDep
		State oldState = currentState;
		if (currentState != State::FAILURE && failedDepCount > 0)
		{
			currentState = State::FAILURE;
			cb.Invoke(oldState, currentState, *this);
		}

		if (failedDepCount == 0)
		{
			if (currentState != State::READY && desiredState != State::EMPTY && emptyDepCount == 0)
			{
				currentState = State::READY;
				cb.Invoke(oldState, currentState, *this);
			}

			if (currentState != State::EMPTY && emptyDepCount > 0)
			{
				currentState = State::EMPTY;
				cb.Invoke(oldState, currentState,*this);
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