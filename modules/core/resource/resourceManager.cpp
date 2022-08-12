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
			Reload(*res);
	}

	void ResourceFactory::Reload(Resource& res)
	{
		// Unload resource
		if (res.currentState != Resource::State::EMPTY)
			res.DoUnload();
		else if (res.desiredState == Resource::State::READY)
			return;

		// Load resource
		if (resManager->OnBeforeLoad(res) == ResourceManager::LoadHook::Action::DEFERRED)
		{
			ASSERT(res.IsHooked() == false);
			res.SetHooked(true);
			res.desiredState = Resource::State::READY;
			res.AddReference(); // Hook
		}
		else
		{
			res.DoLoad();
		}
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

		res->DoUnload();

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

		for (auto storage : storageMap)
		{
			if (storage != nullptr)
			{
				storage->Unload();
				CJING_SAFE_DELETE(storage);
			}
		}
		storageMap.clear();

		// Uninit resource loading
		ContentLoadingManager::Uninitialize();

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

	void ResourceManager::ReloadAll()
	{
		while (fileSystem->HasWork())
			fileSystem->ProcessAsync();

		Array<Resource*> toReload;
		for (auto kvp : factoryTable)
		{
			auto& resources = kvp.second->GetResourceTable();
			for (auto i : resources)
			{
				if (i.second->IsReady())
				{
					i.second->DoUnload();
					toReload.push_back(i.second);
				}
			}
		}

		for (auto res : toReload)
			res->DoLoad();
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

		// Process storages
		// Relase resource storage if it should dispose (no reference and no lock)
		for (auto it = storageMap.begin(); it != storageMap.end(); ++it)
		{
			if (it.value()->ShouldDispose())
				toRemoved.push_back({ it.key(), it.value() });
			else
				it.value()->Tick();
		}
		for (auto kvp : toRemoved)
		{
			storageMap.erase(kvp.first);
			kvp.second->Unload();
			CJING_SAFE_DELETE(kvp.second);
		}
		toRemoved.clear();
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
		ResourceStorage* ret = nullptr;
		auto it = storageMap.find(path.GetHashValue());
		if (!it.isValid())
		{
			auto newStorage = CJING_NEW(ResourceStorage)(path, *this);
			storageMap.insert(path.GetHashValue(), newStorage);
			ret = newStorage;
		}
		else
		{
			ret = it.value();
		}

		return ResourceStorageRef(ret);
	}

	void ResourceManager::LoadHook::ContinueLoad(Resource& res)
	{
		ASSERT(res.IsEmpty());
		res.RemoveReference();
		res.SetHooked(false);
		res.desiredState = Resource::State::EMPTY;
		res.DoLoad();
	}
}