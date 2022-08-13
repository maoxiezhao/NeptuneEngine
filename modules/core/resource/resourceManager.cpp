#include "resourceManager.h"
#include "resourceLoading.h"
#include "core\filesystem\filesystem.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
	ResourceFactory::ResourceFactory() :
		resManager(nullptr)
	{
	}

	ResourceFactory::~ResourceFactory()
	{
		ASSERT(resources.empty());
	}

	void ResourceFactory::Initialize(ResourceType type, ResourceManager& resManager_)
	{
		resManager_.RegisterFactory(type, this);
		resType = type;
		resManager = &resManager_;
	}

	void ResourceFactory::Uninitialize()
	{
		exiting = true;

		// Unload remaining resources
		{
			ScopedMutex lock(resLock);
			for (auto& kvp : resources)
			{
				Resource* res = kvp.second;
				UnloadResoruce(res);
			}
			resources.clear();
		}

		resManager->UnregisterFactory(resType);
		resManager = nullptr;
	}
	
	void ResourceFactory::Update(F32 dt)
	{
		// Refresh state of resoruces
		{
			ScopedMutex lock(resLock);
			for (auto& kvp : resources)
			{
				if (kvp.second->IsStateDirty())
					kvp.second->CheckState();
			}
		}
		// Broadcast OnLoaded 
		{
			ScopedMutex lock(loadedResLock);
			for (auto res : loadedResources)
				res->OnLoaded();
			loadedResources.clear();
		}
	}

	void ResourceFactory::Reload(const Path& path)
	{
		Resource* res = GetResource(path);
		if (res)
			ReloadResource(res);
	}

	void ResourceFactory::Reload(Resource* res)
	{
		ReloadResource(res);
	}

	void ResourceFactory::RemoveUnreferenced()
	{
		ScopedMutex lock(resLock);
		for (auto& kvp : resources)
		{
			if (kvp.second->GetReference() <= 0)
				toRemoved.push_back(kvp.second);
		}

		for (auto res : toRemoved)
		{
			auto it = resources.find(res->GetPath().GetHashValue());
			if (it != resources.end())
			{
				if (res->GetReference() <= 0)
				{
					UnloadResoruce(res);
					resources.erase(it);
				}
			}
		}
		toRemoved.clear();
	}

	void ResourceFactory::OnResourceLoaded(Resource* res)
	{
		ScopedMutex lock(loadedResLock);
		loadedResources.push_back(res);
	}

	Resource* ResourceFactory::GetResource(const Path& path)
	{
		if (path.IsEmpty())
			return nullptr;

		ScopedMutex lock(resLock);
		auto it = resources.find(path.GetHashValue());
		if (it != resources.end())
			return it->second;

		return nullptr;
	}

	ResourceFactory::ResourceTable& ResourceFactory::GetResourceTable()
	{
		return resources;
	}

	void ResourceFactory::DestroyResource(Resource* res)
	{
		ASSERT(res);
		if (exiting)
			res->DeleteObjectNow();
		else
			res->DeleteObject();
	}

	void ResourceFactory::ReloadResource(Resource* res)
	{
	}

	Resource* ResourceFactory::LoadResource(Resource* res)
	{
		if (res->IsEmpty() && res->desiredState == Resource::State::EMPTY)
		{
			if (resManager->OnBeforeLoad(*res) == ResourceManager::LoadHook::Action::DEFERRED)
			{
				ASSERT(res->IsHooked() == false);
				res->SetHooked(true);
				res->desiredState = Resource::State::READY;
				res->AddReference(); // Hook
				return res;
			}
			res->DoLoad();
		}
		return res;
	}

	void ResourceFactory::UnloadResoruce(Resource* res)
	{
		if (res == nullptr)
			return;

		if (!res->IsOwnedBySelf())
			DestroyResource(res);

		loadedResources.erase(res);
	}

	ResourceManager::ResourceManager() = default;
	ResourceManager::~ResourceManager() = default;

	void ResourceManager::Initialize(FileSystem& fileSystem_)
	{
		ASSERT(isInitialized == false);

		// Init resource loading
		ContentLoadingManager::Initialize();

		fileSystem = &fileSystem_;
		isInitialized = true;
	}

	void ResourceManager::Uninitialzie()
	{
		ASSERT(isInitialized == true);

		// Uninit resource loading
		ContentLoadingManager::Uninitialize();

		// Flush pending objects
		ObjectService::FlushNow();

		isInitialized = false;
	}

	Resource* ResourceManager::LoadResource(ResourceType type, const Path& path)
	{
		ASSERT(isInitialized);
		ASSERT(!path.IsEmpty());

		ResourceFactory* factory = GetFactory(type);
		if (factory == nullptr)
			return nullptr;

		return factory->LoadResource(path);
	}

	void ResourceManager::Reload(const Path& path)
	{
		for (auto kvp : factoryTable)
			kvp.second->Reload(path);
	}

	void ResourceManager::Update(F32 dt)
	{
		ASSERT(isInitialized);
		PROFILE_FUNCTION();
		for (auto& kvp : factoryTable)
			kvp.second->Update(dt);
	}

	static F32 UnloadCheckInterval = 0.5f;
	void ResourceManager::LateUpdate()
	{
		ASSERT(isInitialized);
		PROFILE_FUNCTION();

		// Remove unreferenced
		// Keep a time interval, resources may be loaded immediately after unloading
		const F32 now = timer.GetTimeSinceStart();
		if (now - lastUnloadCheckTime >= UnloadCheckInterval)
		{
			for (auto& kvp : factoryTable)
				kvp.second->RemoveUnreferenced();

			lastUnloadCheckTime = now;
		}
	}

	ResourceFactory* ResourceManager::GetFactory(ResourceType type)
	{
		ScopedMutex lock(factoryMutex);
		auto it = factoryTable.find(type.GetHashValue());
		if (it != factoryTable.end())
			return it->second;
		return nullptr;
	}

	ResourceManager::FactoryTable& ResourceManager::GetAllFactories()
	{
		return factoryTable;
	}

	void ResourceManager::RegisterFactory(ResourceType type, ResourceFactory* factory)
	{
		ASSERT(isInitialized);
		factoryTable[type.GetHashValue()] = factory;
	}

	void ResourceManager::UnregisterFactory(ResourceType type)
	{
		ASSERT(isInitialized);
		factoryTable.erase(type.GetHashValue());
	}

	void ResourceManager::SetLoadHook(LoadHook* hook)
	{
		loadHook = hook;
	}

	ResourceManager::LoadHook::Action ResourceManager::OnBeforeLoad(Resource& res)
	{
		return loadHook != nullptr ? loadHook->OnBeforeLoad(res) : LoadHook::Action::IMMEDIATE;
	}

	ResourceStorageRef ResourceManager::GetStorage(const Path& path)
	{
		return StorageManager::GetStorage(path, *this);
	}

	void ResourceManager::LoadHook::ContinueLoad(Resource& res)
	{
		ASSERT(res.IsEmpty());
		ResourceFactory& factory = res.GetResourceFactory();
		factory.ContinuleLoadResource(&res);
	}
}