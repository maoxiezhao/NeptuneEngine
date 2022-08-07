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

	struct VULKAN_TEST_API CompiledResourceHeader
	{
		static constexpr U32 MAGIC = 'FUCK';
		static constexpr U32 VERSION = 0x01;

		U32 magic = MAGIC;
		U32 version = 0;
		U64 originSize = 0;
		bool isCompressed = false;
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
		bool IsReady()const {
			return currentState == State::READY;
		}
		bool IsFailure()const {
			return currentState == State::FAILURE;
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
	
		void DoLoad();
		void DoUnload();
		void CheckState();

		virtual void OnCreated(State state);
		virtual bool OnLoaded(U64 size, const U8* mem) = 0;
		virtual void OnUnLoaded() = 0;

#if DEBUG
		virtual bool NeedExport()const;
#endif

		ResourceFactory& resFactory;
		State desiredState;
		U32 failedDepCount;
		U32 emptyDepCount;
		U64 resSize;

	private:
		friend struct ResourceDeleter;

		Resource(const Resource&) = delete;
		void operator=(const Resource&) = delete;
		
		void OnFileLoaded(U64 size, const U8* mem, bool success);
		void OnStateChanged(State oldState, State newState, Resource& res);

		Path path;
		bool hooked = false;
		bool ownedBySelf = false;
		State currentState;
		StateChangedCallback cb;
		AsyncLoadHandle asyncHandle;
	};

	template<typename T>
	using ResPtr = Util::IntrusivePtr<T>;

#define DECLARE_RESOURCE(CLASS_NAME)															\
	static const ResourceType ResType;                                                          \
	ResourceType GetType()const override { return ResType; };

#define DEFINE_RESOURCE(CLASS_NAME)																\
	const ResourceType CLASS_NAME::ResType(#CLASS_NAME);
}