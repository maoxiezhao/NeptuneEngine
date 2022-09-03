#pragma once

#include "core\common.h"
#include "core\resource\resourceLoading.h"
#include "core\filesystem\filesystem.h"
#include "core\utils\guid.h"
#include "resourceHeader.h"

namespace VulkanTest
{
	class ResourceFactory;
	class ResourceManager;
	class LoadResourceTask;

	// Resource base class
	class VULKAN_TEST_API Resource : public Object
	{
	public:
		friend class ResourceFactory;
		friend class ResourceManager;

		virtual ~Resource();
		virtual ResourceType GetType()const = 0;

		// Resource state
		enum class State
		{
			EMPTY = 0,
			READY,		// Loaded success
			FAILURE,
		};
		State GetState()const {
			return currentState;
		}
		bool IsEmpty()const {
			return currentState == State::EMPTY;
		}
		bool IsFailure()const {
			return currentState == State::FAILURE;
		}
		virtual bool IsReady()const {
			return currentState == State::READY;
		}

		Guid GetGUID()const {
			return guid;
		}

		const Path& GetPath()const {
			return path;
		}

		Path GetStoragePath()const;

		U64 Size() const { 
			return resSize;
		}

		bool WaitForLoaded(F32 seconds = 30.0f)const;
		void Refresh();
		void AddDependency(Resource& depRes);
		void RemoveDependency(Resource& depRes);

		void SetHooked(bool isHooked) {
			hooked = isHooked;
		}

		bool IsHooked()const {
			return hooked;
		}

		void SetOwnedBySelf(bool ownedBySelf_) {
			ownedBySelf = ownedBySelf_;
		}

		bool IsOwnedBySelf()const {
			return ownedBySelf;
		}

		bool IsStateDirty()const {
			return isStateDirty;
		}

		FORCE_INLINE void AddReference() {
			AtomicIncrement(&refCount);
		}

		FORCE_INLINE void RemoveReference() {
			AtomicDecrement(&refCount);
		}

		I32 GetReference() const;

		void SetIsReloading(bool isReloading_) {
			isReloading = isReloading_;
		}

		bool IsReloading()const {
			return isReloading;
		}

		bool IsLoaded()const {
			return isLoaded;
		}

		// Called by ObjectService
		void OnDelete()override;

		ResourceFactory& GetResourceFactory();
		ResourceManager& GetResourceManager();

		using EventCallback = DelegateList<void(Resource*)>;
		EventCallback OnLoadedCallback;
		EventCallback OnReloadingCallback;
		EventCallback OnUnloadedCallback;

	protected:
		friend class LoadResourceTask;

		Resource(const Path& path_, ResourceFactory& resFactory_);
	
		void DoLoad();
		void Reload();
		void CheckState();
		void OnLoaded();
		void OnUnLoaded();

		virtual bool LoadResource() = 0;
		virtual void OnCreated(State state);

		virtual ContentLoadingTask* CreateLoadingTask();
		bool LoadingFromTask(LoadResourceTask* task);
		void OnLoadedFromTask(LoadResourceTask* task);

		using StateChangedCallbackType = DelegateList<void(State, State, Resource&)>;
		StateChangedCallbackType StateChangedCallback;

	protected:
		virtual bool Load() = 0;
		virtual void Unload() = 0;

		ResourceFactory& resFactory;
		State desiredState;
		U32 failedDepCount;
		U32 emptyDepCount;
		U64 resSize;
		ContentLoadingTask* loadingTask;
		volatile I64 refCount;
		Mutex mutex;

		bool isStateDirty = false;
		bool isReloading = false;
		bool isLoaded = false;

	private:
		friend struct ResourceDeleter;

		Resource(const Resource&) = delete;
		void operator=(const Resource&) = delete;
		
		void OnStateChanged(State oldState, State newState, Resource& res);

		Guid guid;
		Path path;
		bool hooked = false;
		bool ownedBySelf = false;
		State currentState;
	};

#define DECLARE_RESOURCE(CLASS_NAME)															\
	static const ResourceType ResType;                                                          \
	ResourceType GetType()const override { return ResType; };

#define DEFINE_RESOURCE(CLASS_NAME)																\
	const ResourceType CLASS_NAME::ResType(#CLASS_NAME);
}