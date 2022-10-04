#pragma once

#include "resource.h"
#include "resourceReference.h"
#include "resourcesCache.h"
#include "storage\resourceStorage.h"
#include "storage\storageManager.h"
#include "core\collections\hashMap.h"
#include "core\platform\timer.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
	class VULKAN_TEST_API ResourceFactory
	{
	public:
		friend class Resource;
		friend class ResourceManager;

		ResourceFactory();
		virtual ~ResourceFactory();

		virtual void Initialize(ResourceType type);
		virtual void Uninitialize();

	protected:
		virtual Resource* NewResource(const ResourceInfo& info) = 0;
		virtual Resource* CreateResource(const ResourceInfo& info) = 0;
		virtual Resource* CreateTemporaryResource(const ResourceInfo& info) = 0;

	protected:
		ResourceType resType;
		bool exiting = false;
	};

	class VULKAN_TEST_API ResourceManager
	{
	public:
		using FactoryTable = std::unordered_map<U64, ResourceFactory*>;

		template<typename T>
		static T* LoadResource(const Path& path)
		{
			return static_cast<T*>(LoadResource(T::ResType, path));
		}

		template<typename T>
		static T* LoadResource(const Guid& guid)
		{
			return static_cast<T*>(LoadResource(T::ResType, guid));
		}

		template<typename T>
		static T* LoadResourceInternal(const Path& path)
		{
			return static_cast<T*>(LoadResourceInternal(T::ResType, path));
		}

		static Resource* LoadResource(ResourceType type, const Guid& guid);
		static Resource* LoadResource(ResourceType type, const Path& path);
		static Resource* LoadResourceInternal(ResourceType type, const Path& path);

		static void UnloadResoruce(Resource* res);
		static void ReloadResource(const Path& path);
		static Resource* CreateTemporaryResource(ResourceType type);
		static void AddTemporaryResource(Resource* res);
		static void DeleteResource(Resource* res);
		static void DeleteResource(const Path& path);

		static ResourceStorageRef GetStorage(const Path& path);
		static bool GetResourceInfo(const Guid& guid, ResourceInfo& info);
		static bool GetResourceInfo(const Path& path, ResourceInfo& info);
		static Resource* GetResource(const Path& path);
		static Resource* GetResource(const Guid& guid);

		// Resource factory
		static ResourceFactory* GetFactory(ResourceType type);
		static FactoryTable& GetAllFactories();
		static void RegisterFactory(ResourceType type, ResourceFactory* factory);
		static void UnregisterFactory(ResourceType type);
		static ResourcesCache& GetCache();

		// Resource load hook
		struct VULKAN_TEST_API LoadHook
		{
			enum class Action
			{
				IMMEDIATE,
				DEFERRED
			};
			virtual ~LoadHook() {}
			virtual Action OnBeforeLoad(Resource& res) = 0;
			void ContinueLoad(Resource& res);
		};
		static void SetLoadHook(LoadHook* hook);
		static LoadHook::Action OnBeforeLoad(Resource& res);

	private:
		friend class Resource;

		static Resource* LoadResourceImpl(ResourceType type, const Guid& guid);
		static Resource* ContinueLoadResource(ResourceType type, const Guid& guid);

		static void OnResourceLoaded(Resource* res);
		static void TryCallOnResourceLoaded(Resource* res);
		static void OnResourceUnload(Resource* res);
	};
}