#pragma once

#include "resource.h"
#include "resourceStorage.h"
#include "resourceReference.h"
#include "storageManager.h"
#include "resourcesCache.h"
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

		virtual void Initialize(ResourceType type, ResourceManager& resManager_);
		virtual void Uninitialize();

		ResourceManager& GetResourceManager() {
			return *resManager;
		}

	protected:
		virtual Resource* NewResource(const ResourceInfo& info) = 0;
		virtual Resource* CreateResource(const ResourceInfo& info) = 0;
		virtual Resource* CreateTemporaryResource(const ResourceInfo& info) = 0;

	protected:
		ResourceType resType;
		ResourceManager* resManager;
		bool exiting = false;
	};

	class VULKAN_TEST_API ResourceManager
	{
	public:
		using FactoryTable = std::unordered_map<U64, ResourceFactory*>;

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

		ResourceManager();
		virtual ~ResourceManager();

		void Initialize(class FileSystem& fileSystem_);
		void Uninitialzie();
		void Update(F32 dt);
		void LateUpdate();

		template<typename T>
		T* LoadResource(const Path& path)
		{
			return static_cast<T*>(LoadResource(T::ResType, path));
		}

		template<typename T>
		T* LoadResource(const Guid& guid)
		{
			return static_cast<T*>(LoadResource(T::ResType, guid));
		}

		Resource* LoadResource(ResourceType type, const Guid& guid);
		Resource* LoadResource(ResourceType type, const Path& path);
		void UnloadResoruce(Resource* res);
		void ReloadResource(const Path& path);
		Resource* CreateTemporaryResource(ResourceType type);
		void AddTemporaryResource(Resource* res);
		void DeleteResource(Resource* res);
		void DeleteResource(const Path& path);

		ResourceStorageRef GetStorage(const Path& path);
		bool GetResourceInfo(const Guid& guid, ResourceInfo& info);
		bool GetResourceInfo(const Path& path, ResourceInfo& info);
		Resource* GetResource(const Path& path);
		Resource* GetResource(const Guid& guid);

		// Resource factory
		ResourceFactory* GetFactory(ResourceType type);
		FactoryTable& GetAllFactories();
		void RegisterFactory(ResourceType type, ResourceFactory* factory);
		void UnregisterFactory(ResourceType type);

		class FileSystem* GetFileSystem() {
			return fileSystem;
		}

		ResourcesCache& GetCache() {
			return cache;
		}

		void SetLoadHook(LoadHook* hook);
		LoadHook::Action OnBeforeLoad(Resource& res);

	private:
		Resource* LoadResourceImpl(ResourceType type, const Guid& guid);
		Resource* ContinueLoadResource(ResourceType type, const Guid& guid);

		void OnResourceLoaded(Resource* res);
		void TryCallOnResourceLoaded(Resource* res);
		void OnResourceUnload(Resource* res);

	private:
		friend class Resource;

		class FileSystem* fileSystem = nullptr;
		Mutex factoryMutex;
		FactoryTable factoryTable;
		F32 lastUnloadCheckTime = 0.0f;
		Timer timer;
		ResourcesCache cache;
		LoadHook* loadHook = nullptr;
		bool isInitialized = false;

		// All resources
		Mutex resourceMutex;
		HashMap<Guid, Resource*> resources;

		// Loading resources
		Mutex loadingResourcesMutex;
		Array<Guid> loadingResources;

		// To removed resources
		Mutex toRemovedLock;
		Array<Resource*> toRemoved;

		// Onloaded resources
		Mutex loadedResLock;
		Array<Resource*> onLoadedResources;
	};
}