#include "resourceManager.h"
#include "core\filesystem\filesystem.h"

namespace VulkanTest
{
	ResourceFactory::ResourceFactory() :
		isUnloadEnable(true),
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
		for (auto& kvp : resources)
		{
			Resource* res = kvp.second;
			if (!res->IsEmpty())
				Logger::Error("Resource %s leak", res->GetPath().c_str());
			
			if (!res->IsOwnedBySelf())
				DestroyResource(res);
		}
		resources.clear();

		resManager->UnregisterFactory(resType);
		resManager = nullptr;
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
		if (isUnloadEnable == false)
			return;

		std::vector<Resource*> toRemoved;
		for (auto& kvp : resources)
		{
			if (kvp.second->GetReference() == 0)
				toRemoved.push_back(kvp.second);
		}

		for (auto res : toRemoved)
		{
			auto it = resources.find(res->GetPath().GetHashValue());
			if (it != resources.end())
				resources.erase(it);
		}
	}

	Resource* ResourceFactory::GetResource(const Path& path)
	{
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

	ResourceManager::ResourceManager() = default;
	ResourceManager::~ResourceManager() = default;

	void ResourceManager::Initialize(FileSystem& fileSystem_)
	{
		ASSERT(isInitialized == false);
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

		isInitialized = false;
	}

	void ResourceManager::RemoveUnreferenced()
	{
		ASSERT(isInitialized);
		for (auto& kvp : factoryTable)
			kvp.second->RemoveUnreferenced();
	}

	void ResourceManager::UpdateResourceStorages()
	{
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

	ResourceFactory* ResourceManager::GetFactory(ResourceType type)
	{
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
			auto newStorage = CJING_NEW(ResourceStorage)(path, *fileSystem);
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
		res.Release();
		res.SetHooked(false);
		res.desiredState = Resource::State::EMPTY;
		res.DoLoad();
	}
}