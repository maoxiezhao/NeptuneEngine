#pragma once

#include "resource.h"

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
		void RemoveUnreferenced();

		Resource* GetResource(const Path& path);

		bool IsUnloadEnable()const {
			return isUnloadEnable;
		}

		ResourceManager& GetResourceManager() {
			return *resManager;
		}

	protected:
		Resource* LoadResource(const Path& path);

		virtual Resource* CreateResource(const Path& path) = 0;
		virtual void DestroyResource(Resource* res) = 0;

	private:
		ResourceTable resources;
		bool isUnloadEnable;
		ResourceType resType;
		ResourceManager* resManager;
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
		void RemoveUnreferenced();

		// TODO: to remove
		template<typename T>
		T* LoadResource(const Path& path)
		{
			return static_cast<T*>(LoadResource(T::ResType, path));
		}

		template<typename T>
		ResPtr<T> LoadResourcePtr(const Path& path)
		{
			return ResPtr<T>(LoadResource<T>(path));
		}

		Resource* LoadResource(ResourceType type, const Path& path);
		
		ResourceFactory* GetFactory(ResourceType type);
		FactoryTable& GetAllFactories();
		void RegisterFactory(ResourceType type, ResourceFactory* factory);
		void UnregisterFactory(ResourceType type);

		class FileSystem* GetFileSystem() {
			return fileSystem;
		}

		void SetLoadHook(LoadHook* hook);
		LoadHook::Action OnBeforeLoad(Resource& res);

	private:
		friend class Resource;

		class FileSystem* fileSystem = nullptr;
		FactoryTable factoryTable;
		bool isInitialized = false;
		LoadHook* loadHook = nullptr;
	};
}