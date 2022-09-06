#pragma once

#include "core\common.h"
#include "core\globals.h"
#include "core\resource\resourceLoading.h"
#include "core\filesystem\filesystem.h"
#include "core\utils\guid.h"
#include "resourceHeader.h"
#include "resourceInfo.h"

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
		friend class ResourceStorage;

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

		U64 Size() const { 
			return resSize;
		}

		bool WaitForLoaded(F32 seconds = 30.0f);
		void Refresh();
		void Reload();
		void AddDependency(Resource& depRes);
		void RemoveDependency(Resource& depRes);

		void SetIsTemporary();
		bool IsTemporary()const {
			return isTemporary;
		}

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

		ResourceInfo GetResourceInfo()const {
			return ResourceInfo(guid, GetType(), GetPath());
		}

		// Called by ObjectService
		void OnDelete()override;

		ResourceManager& GetResourceManager();

		using EventCallback = DelegateList<void(Resource*)>;
		EventCallback OnLoadedCallback;
		EventCallback OnReloadingCallback;
		EventCallback OnUnloadedCallback;

	protected:
		friend class LoadResourceTask;

		Resource(const ResourceInfo& info, ResourceManager& resManager_);
	
		void DoLoad();
		void CheckState();

		virtual bool LoadResource() = 0;
		virtual void OnCreated(State state);
		virtual void CancelStreaming();

		virtual ContentLoadingTask* CreateLoadingTask();
		bool LoadingFromTask(LoadResourceTask* task);
		void OnLoadEndFromTask(LoadResourceTask* task);

		using StateChangedCallbackType = DelegateList<void(State, State, Resource&)>;
		StateChangedCallbackType StateChangedCallback;

	protected:
		virtual bool Load() = 0;
		virtual void Unload() = 0;

		State desiredState;
		U32 failedDepCount;
		U32 emptyDepCount;
		U64 resSize;

		ResourceManager& resManager;
		ContentLoadingTask* loadingTask;
		volatile I64 refCount;
		Mutex mutex;

		bool isStateDirty = false;
		bool isReloading = false;
		bool isLoaded = false;
		bool toDelete = false;

	private:
		friend struct ResourceDeleter;

		Resource(const Resource&) = delete;
		void operator=(const Resource&) = delete;
		
		void OnLoadedMainThread();
		void OnUnLoadedMainThread();
		void OnStateChanged(State oldState, State newState, Resource& res);

	private:
		Guid guid;
		Path path;
		bool hooked = false;
		bool ownedBySelf = false;
		bool isTemporary = false;
		State currentState;
	};

#define DECLARE_RESOURCE(CLASS_NAME)															\
	static const ResourceType ResType;                                                          \
	ResourceType GetType()const override { return ResType; };

#define DEFINE_RESOURCE(CLASS_NAME)																\
	const ResourceType CLASS_NAME::ResType(#CLASS_NAME);
}