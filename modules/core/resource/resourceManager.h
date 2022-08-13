#pragma once

#include "resource.h"
#include "resourceStorage.h"
#include "resourceReference.h"
#include "storageManager.h"
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
		using ResourceTable = std::unordered_map<U64, Resource*>;
		
		ResourceFactory();
		virtual ~ResourceFactory();

		void Initialize(ResourceType type, ResourceManager& resManager_);
		void Uninitialize();
		void Update(F32 dt);
		void Reload(const Path& path);
		void Reload(Resource* res);
		void RemoveUnreferenced();
		void OnResourceLoaded(Resource* res);

		Resource* GetResource(const Path& path);
		ResourceTable& GetResourceTable();
		ResourceManager& GetResourceManager() {
			return *resManager;
		}

	protected:
		virtual Resource* LoadResource(const Path& path) = 0;
		virtual void ContinuleLoadResource(Resource* res) = 0;
		virtual Resource* CreateResource(const Path& path) = 0;
		virtual void DestroyResource(Resource* res);
		virtual void ReloadResource(Resource* res) = 0;

		Resource* LoadResource(Resource* res);
		void UnloadResoruce(Resource* res);

	protected:
		ResourceTable resources;
		ResourceType resType;
		ResourceManager* resManager;
		Mutex resLock;
		Array<Resource*> toRemoved;
		Mutex loadedResLock;
		Array<Resource*> loadedResources;
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

		template<typename T>
		ResPtr<T> LoadResourcePtr(const Path& path)
		{
			return ResPtr<T>(LoadResource<T>(path));
		}

		ResPtr<Resource> LoadResourcePtr(ResourceType type, const Path& path)
		{
			return ResPtr<Resource>(LoadResource(type, path));
		}

		Resource* LoadResource(ResourceType type, const Path& path);
		
		void Reload(const Path& path);
		void Update(F32 dt);
		void LateUpdate();

		ResourceFactory* GetFactory(ResourceType type);
		FactoryTable& GetAllFactories();
		void RegisterFactory(ResourceType type, ResourceFactory* factory);
		void UnregisterFactory(ResourceType type);

		class FileSystem* GetFileSystem() {
			return fileSystem;
		}

		void SetLoadHook(LoadHook* hook);
		LoadHook::Action OnBeforeLoad(Resource& res);

		ResourceStorageRef GetStorage(const Path& path);

	private:
		template<typename T>
		T* LoadResource(const Path& path)
		{
			return static_cast<T*>(LoadResource(T::ResType, path));
		}

	private:
		friend class Resource;

		class FileSystem* fileSystem = nullptr;
		Mutex factoryMutex;
		FactoryTable factoryTable;
		bool isInitialized = false;
		LoadHook* loadHook = nullptr;

		F32 lastUnloadCheckTime = 0.0f;
		Timer timer;
	};
}