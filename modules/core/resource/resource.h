#pragma once

#include "core\common.h"
#include "core\filesystem\filesystem.h"
#include "core\utils\path.h"
#include "core\utils\stringID.h"
#include "core\utils\delegate.h"

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
		U32 originSize = 0;
		bool isCompressed = false;
	};

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
		bool IsReady()const {
			return currentState == State::READY;
		}
		bool IsFailure()const {
			return currentState == State::FAILURE;
		}

		const Path& GetPath()const {
			return path;
		}

		U32 GetRefCount()const {
			return refCount;
		}
		void IncRefCount();
		void DecRefCount();
		void Refresh();

		ResourceFactory& GetResourceFactoyr() {
			return resFactory;
		}

		using StateChangedCallback = DelegateList<void(State, State)>;

	protected:
		Resource(const Path& path_, ResourceFactory& resFactory_);
	
		void DoLoad();
		void DoUnload();
		void CheckState();

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
		Resource(const Resource&) = delete;
		void operator=(const Resource&) = delete;
		
		void OnFileLoaded(U64 size, const U8* mem, bool success);
		void OnStateChanged(State oldState, State newState);

		Path path;
		U32 refCount;
		State currentState;
		StateChangedCallback cb;
		AsyncLoadHandle asyncHandle;
	};

#define DECLARE_RESOURCE(CLASS_NAME)															\
	static const ResourceType ResType;                                                          \
	ResourceType GetType()const override { return ResType; };

#define DEFINE_RESOURCE(CLASS_NAME)																\
	const ResourceType CLASS_NAME::ResType(#CLASS_NAME);
}