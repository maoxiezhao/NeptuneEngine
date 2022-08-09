#pragma once

#include "core\common.h"
#include "core\filesystem\filesystem.h"
#include "core\utils\path.h"
#include "core\utils\stringID.h"
#include "core\utils\delegate.h"
#include "core\utils\intrusivePtr.hpp"

namespace VulkanTest
{
	class ResourceFactory;
	class ResourceManager;

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
	struct ResourceDeleter
	{
		void operator()(Resource* res);
	};

	class VULKAN_TEST_API Resource : public Util::IntrusivePtrEnabled<Resource, ResourceDeleter>
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

		ResourceFactory& GetResourceFactory() {
			return resFactory;
		}

		ResourceManager& GetResourceManager();

		using StateChangedCallback = DelegateList<void(State, State, Resource&)>;
		StateChangedCallback& GetStateChangedCallback() { return cb; }

	protected:
		Resource(const Path& path_, ResourceFactory& resFactory_);
	
		virtual void DoLoad();
		virtual void DoUnload();

		virtual void OnCreated(State state);
		virtual bool OnLoaded() = 0;
		virtual void OnUnLoaded() = 0;

		void CheckState();

		ResourceFactory& resFactory;
		State desiredState;
		U32 failedDepCount;
		U32 emptyDepCount;
		U64 resSize;

	private:
		friend struct ResourceDeleter;

		Resource(const Resource&) = delete;
		void operator=(const Resource&) = delete;
		
		void OnStateChanged(State oldState, State newState, Resource& res);

		Path path;
		bool hooked = false;
		bool ownedBySelf = false;
		State currentState;
		StateChangedCallback cb;
	};

	template<typename T>
	using ResPtr = Util::IntrusivePtr<T>;

#define DECLARE_RESOURCE(CLASS_NAME)															\
	static const ResourceType ResType;                                                          \
	ResourceType GetType()const override { return ResType; };

#define DEFINE_RESOURCE(CLASS_NAME)																\
	const ResourceType CLASS_NAME::ResType(#CLASS_NAME);
}