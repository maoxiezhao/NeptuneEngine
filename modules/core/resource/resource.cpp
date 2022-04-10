#include "resource.h"
#include "resourceManager.h"

namespace VulkanTest
{
	const ResourceType ResourceType::INVALID_TYPE("");

	ResourceType::ResourceType(const char* typeName)
	{
		ASSERT(typeName[0] == 0 || (typeName[0] >= 'a' && typeName[0] <= 'z'));
		type = StringID(typeName);
	}

	Resource::~Resource() = default;

	void Resource::IncRefCount()
	{
		refCount++;
	}

	void Resource::DecRefCount()
	{
		ASSERT(refCount > 0);
		if (--refCount == 0 && resFactory.IsUnloadEnable())
			DoUnload();
	}

	void Resource::Refresh()
	{
		if (currentState == State::EMPTY)
			return;

		State oldState = currentState;
		currentState = State::EMPTY;
		cb.Invoke(oldState, currentState);
		CheckState();
	}

	Resource::Resource(const Path& path_, ResourceFactory& resFactory_) :
		refCount(0),
		emptyDepCount(1),
		failedDepCount(0),
		currentState(State::EMPTY),
		desiredState(State::EMPTY),
		path(path_),
		resFactory(resFactory_)
	{
	}

	void Resource::DoLoad()
	{
		// Check resource has loaded
		if (desiredState == State::READY)
			return;
		desiredState = State::READY;


	}

	void Resource::DoUnload()
	{
		desiredState = State::EMPTY;
		OnUnLoaded();
		ASSERT(emptyDepCount <= 1);

		// Refresh current state
		emptyDepCount = 1;
		failedDepCount = 0;
		CheckState();
	}

	void Resource::CheckState()
	{
		// Refresh the current state according to failedDep and emtpyDep

		State oldState = currentState;
		// Check failure
		if (currentState != State::FAILURE && failedDepCount > 0)
		{
			currentState = State::FAILURE;
			cb.Invoke(oldState, currentState);
		}

		if (failedDepCount == 0)
		{
			// Check ready
			if (currentState != State::READY && desiredState != State::EMPTY && emptyDepCount == 0)
			{
				currentState = State::READY;
				cb.Invoke(oldState, currentState);
			}

			// Check empty
			if (currentState != State::EMPTY && emptyDepCount > 0)
			{
				currentState = State::EMPTY;
				cb.Invoke(oldState, currentState);
			}
		}
	}

	void Resource::OnStateChanged(State oldState, State newState)
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