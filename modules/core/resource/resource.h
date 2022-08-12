#pragma once

#include "core\common.h"
#include "core\resource\resourceLoading.h"
#include "core\filesystem\filesystem.h"
#include "core\utils\path.h"
#include "core\utils\stringID.h"
#include "core\utils\delegate.h"
#include "core\utils\intrusivePtr.hpp"

namespace VulkanTest
{
	class ResourceFactory;
	class ResourceManager;
	class LoadResourceTask;

	// Resource type
	struct VULKAN_TEST_API ResourceType
	{
		ResourceType() = default;
		explicit ResourceType(const char* typeName);
		bool operator !=(const ResourceType& rhs) const { 
			return rhs.type != type; 
		}
		bool operator ==(const ResourceType& rhs) const { 
			return rhs.type == type; 
		}
		bool operator <(const ResourceType& rhs) const { 
			return rhs.type.GetHashValue() < type.GetHashValue();
		}
		U64 GetHashValue()const {
			return type.GetHashValue();
		}

		static const ResourceType INVALID_TYPE;
		StringID type;
	};

	class Resource;
	class VULKAN_TEST_API Resource
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
			READY,	 // Loaded success
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

		const Path& GetPath()const {
			return path;
		}

		U64 Size() const { 
			return resSize;
		}

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

		ResourceFactory& GetResourceFactory();
		ResourceManager& GetResourceManager();

		using EventCallback = DelegateList<void(Resource*)>;
		EventCallback OnLoadedCallback;
		EventCallback OnReloadingCallback;
		EventCallback OnUnloadedCallback;

	protected:
		friend class LoadResourceTask;

		Resource(const Path& path_, ResourceFactory& resFactory_);
	
		virtual void DoLoad();
		virtual void DoUnload();
		virtual bool LoadResource() = 0;

		void CheckState();
		void OnLoaded();
		void OnUnLoaded();

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

	private:
		friend struct ResourceDeleter;

		Resource(const Resource&) = delete;
		void operator=(const Resource&) = delete;
		
		void OnStateChanged(State oldState, State newState, Resource& res);

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