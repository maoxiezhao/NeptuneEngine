#include "resourceManager.h"
#include "loading\resourceLoading.h"
#include "core\filesystem\filesystem.h"
#include "core\profiler\profiler.h"
#include "core\engine.h"

namespace VulkanTest
{
	namespace
	{
		bool isExit = false;
		Mutex factoryMutex;
		ResourceManager::FactoryTable factoryTable;
		F32 lastUnloadCheckTime = 0.0f;
		Timer timer;
		ResourcesCache cache;
		ResourceManager::LoadHook* loadHook = nullptr;

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
	}

	class ResourceManagerServiceImpl : public EngineService
	{
	public:
		ResourceManagerServiceImpl() :
			EngineService("ResourceManagerService", -500)
		{}

		bool Init(Engine& engine) override;
		void Update() override;
		void LateUpdate() override;
		void Uninit() override;
	};
	ResourceManagerServiceImpl ResourceManagerServiceImplInstance;

	ResourceFactory::ResourceFactory()
	{
	}

	ResourceFactory::~ResourceFactory()
	{
	}

	void ResourceFactory::Initialize(ResourceType type)
	{
		ResourceManager::RegisterFactory(type, this);
		resType = type;
	}

	void ResourceFactory::Uninitialize()
	{
		exiting = true;
		ResourceManager::UnregisterFactory(resType);
	}

	Resource* ResourceManager::LoadResource(ResourceType type, const Guid& guid)
	{
		if (!guid.IsValid())
			return nullptr;

		// Check if resource has been already loaded
		Resource* res = GetResource(guid);
		if (res != nullptr)
		{
			if (res->GetType() != type)
			{
				Logger::Warning("Different loaded resource type. Resource: \'%s\'. Expected type: %d", res->GetPath().c_str(), type.GetHashValue());
				return nullptr;
			}
			return res;
		}

		// Check if resource is loading
		loadingResourcesMutex.Lock();
		if (loadingResources.indexOf(guid) != -1)
		{
			loadingResourcesMutex.Unlock();

			// Is necessary to wait for load end?
			while (true)
			{
				loadingResourcesMutex.Lock();
				const bool isLoading = loadingResources.indexOf(guid) != -1;
				loadingResourcesMutex.Unlock();

				if (!isLoading)
					return GetResource(guid);

				Platform::Sleep(0.001f);
			}
		}
		else
		{
			// Mark resource is loading
			loadingResources.push_back(guid);
			loadingResourcesMutex.Unlock();
		}

		// Load resource
		res = LoadResourceImpl(type, guid);

		loadingResourcesMutex.Lock();
		loadingResources.erase(guid);
		loadingResourcesMutex.Unlock();
		return res;
	}

	Resource* ResourceManager::LoadResourceImpl(ResourceType type, const Guid& guid)
	{
		ResourceInfo resInfo;
		if (!GetResourceInfo(guid, resInfo))
		{
			Logger::Warning("Invalid resource id %d", guid.GetHash());
			return nullptr;
		}

		// Get resource factory
		auto factory = GetFactory(type);
		if (factory == nullptr)
		{
			Logger::Warning("Cannot find resource factory %s", resInfo.path.c_str());
			return nullptr;
		}

		// Create target resource
		Resource* res = factory->NewResource(resInfo);
		if (res == nullptr)
		{
			Logger::Warning("Cannot create resource %s", resInfo.path.c_str());
			return nullptr;
		}
		ASSERT(res->GetGUID() == guid);

		// Record resource 
		resourceMutex.Lock();
		resources.insert(guid, res);
		resourceMutex.Unlock();

#if CJING3D_EDITOR
		if (res->IsEmpty() && res->desiredState == Resource::State::EMPTY)
		{
			if (OnBeforeLoad(*res) == ResourceManager::LoadHook::Action::DEFERRED)
			{
				ASSERT(res->IsHooked() == false);
				res->SetHooked(true);
				res->desiredState = Resource::State::READY;
				res->AddReference(); // Hook
				return res;
			}
		}
#endif
		// Resource start loading
		res->DoLoad();

		return res;
	}

	Resource* ResourceManager::LoadResourceInternal(ResourceType type, const Path& internalPath)
	{
		if (internalPath.IsEmpty() || type == ResourceType::INVALID_TYPE)
			return nullptr;

#if CJING3D_EDITOR
		const Path path = Globals::EngineContentFolder / internalPath + RESOURCE_FILES_EXTENSION_WITH_DOT;
		if (!FileSystem::FileExists(path))
		{
			Logger::Error("Missing file %s", path.c_str());
			return nullptr;
		}
#else
		const Path path = Globals::ProjectContentFolder / internalPath + RESOURCE_FILES_EXTENSION_WITH_DOT;
#endif
		Resource* res = LoadResource(type, path);
		if (res == nullptr)
		{
			Logger::Error("Failed to load %s", path.c_str());
			return nullptr;
		}

		return res;
	}

	Resource* ResourceManager::ContinueLoadResource(ResourceType type, const Guid& guid)
	{
		if (!guid.IsValid())
			return nullptr;

		// Resource must is already set in continue load case
		Resource* res = GetResource(guid);
		if (res == nullptr)
		{
			Logger::Warning("Cannot continue to load resource %d", guid.GetHash());
			return nullptr;
		}

		// Resource start loading
		res->DoLoad();

		// Remove from loading queue
		loadingResourcesMutex.Lock();
		loadingResources.erase(guid);
		loadingResourcesMutex.Unlock();

		return res;
	}

	Resource* ResourceManager::LoadResource(ResourceType type, const Path& path)
	{
		ASSERT(!path.IsEmpty());

#if CJING3D_EDITOR
		Resource* res = nullptr;
		ResourceInfo info;
		if (GetResourceInfo(path, info))
		{
			res = GetResource(info.guid);
		}
		else
		{
			info.guid = Guid::New();
			info.type = type;
			info.path = path;
		}

		if (res == nullptr)
		{
			// Check if resource is loading
			loadingResourcesMutex.Lock();
			if (loadingResources.indexOf(info.guid) != -1)
			{
				loadingResourcesMutex.Unlock();

				// Is necessary to wait for load end?
				while (true)
				{
					loadingResourcesMutex.Lock();
					const bool isLoading = loadingResources.indexOf(info.guid) != -1;
					loadingResourcesMutex.Unlock();

					if (!isLoading)
						return GetResource(info.guid);

					Platform::Sleep(0.001f);
				}
			}
			else
			{
				// Mark resource is loading
				loadingResources.push_back(info.guid);
				loadingResourcesMutex.Unlock();
			}

			// Get resource factory
			auto factory = GetFactory(type);
			if (factory == nullptr)
			{
				Logger::Warning("Cannot find resource factory %s", info.path.c_str());
				return nullptr;
			}

			// Create target resource
			res = factory->NewResource(info);
			if (res == nullptr)
			{
				Logger::Warning("Cannot create resource %s", info.path.c_str());
				return nullptr;
			}

			// Record resource
			// TODO check if is necessary
			resourceMutex.Lock();
			resources.insert(res->GetGUID(), res);
			resourceMutex.Unlock();
		}

		if (res->IsEmpty() && res->desiredState == Resource::State::EMPTY)
		{
			if (OnBeforeLoad(*res) == ResourceManager::LoadHook::Action::DEFERRED)
			{
				ASSERT(res->IsHooked() == false);
				res->SetHooked(true);
				res->desiredState = Resource::State::READY;
				res->AddReference(); // Hook
				return res;
	}
			res = ContinueLoadResource(res->GetType(), res->GetGUID());
		}

		return res;
#else
		ResourceInfo info;
		if (!GetResourceInfo(path, info))
			return nullptr;

		return LoadResource(type, info.guid);
#endif
	}

	void ResourceManager::UnloadResoruce(Resource* res)
	{
		if (res == nullptr)
			return;
		
		res->DeleteObject();
	}

	void ResourceManager::ReloadResource(const Path& path)
	{
		auto storage = StorageManager::TryGetStorage(path);
		if (storage && storage->IsLoaded())
			storage->CloseContent();

		if (storage)
			storage->Reload();
	}

	static Path GetTemporaryResourcePath()
	{
		return Path(".export/resources/temporary");
	}

	Resource* ResourceManager::CreateTemporaryResource(ResourceType type)
	{
		auto factory = GetFactory(type);
		if (factory == nullptr)
		{
			Logger::Error("Cannot find temporary resource factory %d", type.GetHashValue());
			return nullptr;
		}

		ResourceInfo info;
		info.guid = Guid::New();
		info.type = type;
		info.path = GetTemporaryResourcePath();
		auto res = factory->CreateTemporaryResource(info);
		if (res == nullptr)
		{
			Logger::Error("Failed to create temporary resource %d", type.GetHashValue());
			return nullptr;
		}
	
		// Set is a temporary resource
		res->SetIsTemporary();

		resourceMutex.Lock();
		resources.insert(info.guid, res);
		resourceMutex.Unlock();

		return res;
	}

	void ResourceManager::AddTemporaryResource(Resource* res)
	{
		ASSERT(res != nullptr);
		ASSERT(res->GetGUID() != Guid::Empty);

		ScopedMutex lock(resourceMutex);
		auto it = resources.find(res->GetGUID());
		if (it.isValid())
		{
			ASSERT(res->IsTemporary());
			return;
		}

		res->SetIsTemporary();
		resources.insert(res->GetGUID(), res);
	}

	void ResourceManager::DeleteResource(Resource* res)
	{
		ScopedMutex lock(resourceMutex);
		if (res == nullptr || res->toDelete)
			return;

		Logger::Info("Delete resources %s ...", res->GetPath());
		res->toDelete = true;
		UnloadResoruce(res);
	}

	void ResourceManager::DeleteResource(const Path& path)
	{
		// If resource has been already loaded, we should unload resource first
		auto res = GetResource(path);
		if (res != nullptr)
		{
			DeleteResource(res);
			return;
		}

		// Remove from resources cache
		ResourceInfo info;
		if (cache.Delete(path, &info))
			Logger::Info("Delete resource %s", path.c_str());

		// Close storage
		auto storage = StorageManager::TryGetStorage(path);
		if (storage && storage->IsLoaded())
			storage->CloseContent();

		// Delete resource file
		if (FileSystem::FileExists(path.c_str()))
			FileSystem::DeleteFile(path.c_str());
	}

	Path ResourceManager::CreateTemporaryResourcePath()
	{
		return Globals::TemporaryFolder / (Guid::New().ToString(Guid::FormatType::N)) + RESOURCE_FILES_EXTENSION_WITH_DOT;
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
		factoryTable[type.GetHashValue()] = factory;
	}

	void ResourceManager::UnregisterFactory(ResourceType type)
	{
		factoryTable.erase(type.GetHashValue());
	}

	ResourcesCache& ResourceManager::GetCache() {
		return cache;
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
		return StorageManager::GetStorage(path);
	}

	bool ResourceManager::GetResourceInfo(const Guid& guid, ResourceInfo& info)
	{
		if (!guid.IsValid())
			return false;

#ifdef CJING3D_EDITOR
		if (cache.Find(guid, info))
			return true;

		return false;
#else
		return cache.Find(guid, info);
#endif
	}

	bool ResourceManager::GetResourceInfo(const Path& path, ResourceInfo& info)
	{
		if (path.IsEmpty())
			return false;

#ifdef CJING3D_EDITOR
		if (cache.Find(path, info))
			return true;

		auto extension = Path::GetExtension(path.ToSpan());
		if (EqualString(extension, RESOURCE_FILES_EXTENSION) ||
			EqualString(extension, PACKAGE_FILES_EXTENSION))
		{
			auto storage = StorageManager::GetStorage(path);
			if (storage)
			{
				cache.Register(storage);
				return cache.Find(path, info);
			}
		}
		else if (EqualString(extension, "json"))
		{
			ASSERT(false);
			return false;
		}

		return false;
#else
		return cache.Find(path, info);
#endif
	}

	Resource* ResourceManager::GetResource(const Path& path)
	{
		if (path.IsEmpty())
			return nullptr;

		ScopedMutex lock(resourceMutex);
		for (auto& res : resources)
		{
			if (res->GetPath() == path)
				return res;
		}
		return nullptr;
	}

	Resource* ResourceManager::GetResource(const Guid& guid)
	{
		Resource* ret = nullptr;
		resourceMutex.Lock();
		resources.tryGet(guid, ret);
		resourceMutex.Unlock();
		return ret;
	}

	void ResourceManager::LoadHook::ContinueLoad(Resource& res)
	{
		ASSERT(res.IsEmpty());
		res.RemoveReference();
		res.SetHooked(false);
		res.desiredState = Resource::State::EMPTY;
		ResourceManager::ContinueLoadResource(res.GetType(), res.GetGUID());
	}

	void ResourceManager::OnResourceLoaded(Resource* res)
	{
		ASSERT(res && res->IsLoaded());
		ScopedMutex lock(loadedResLock);
		onLoadedResources.push_back(res);
	}

	void ResourceManager::TryCallOnResourceLoaded(Resource* res)
	{
		ASSERT(res && res->IsLoaded());
		ScopedMutex lock(loadedResLock);
		int index = onLoadedResources.indexOf(res);
		if (index != -1)
		{
			onLoadedResources.eraseAt(index);
			res->OnLoadedMainThread();
		}
	}

	void ResourceManager::OnResourceUnload(Resource* res)
	{
		if (isExit == false)
			resourceMutex.Lock();

		resources.erase(res->GetGUID());
		onLoadedResources.erase(res);

		if (isExit == false)
			resourceMutex.Unlock();
	}

	VULKAN_TEST_API Resource* LoadResource(ResourceType type, const Guid& guid)
	{
		return ResourceManager::LoadResource(type, guid);
	}

	bool ResourceManagerServiceImpl::Init(Engine& engine)
	{
		// Init resource cache
		cache.Initialize();

		// Init resource loading
		ContentLoadingManager::Initialize();

		initialized = true;
		return true;
	}

	void ResourceManagerServiceImpl::Update()
	{
		PROFILE_FUNCTION();

		// Refresh state of resoruces
		{
			ScopedMutex lock(resourceMutex);
			for (auto res : resources)
			{
				if (res->IsStateDirty())
					res->CheckState();
			}
		}
		// Broadcast OnLoaded 
		{
			ScopedMutex lock(loadedResLock);
			for (auto res : onLoadedResources)
				res->OnLoadedMainThread();
			onLoadedResources.clear();
		}
	}

	static F32 UnloadCheckInterval = 0.5f;
	void ResourceManagerServiceImpl::LateUpdate()
	{
		PROFILE_FUNCTION();

		// Remove unreferenced
		// Keep a time interval, resources may be loaded immediately after unloading
		const F32 now = timer.GetTimeSinceStart();
		if (now - lastUnloadCheckTime >= UnloadCheckInterval)
		{
			lastUnloadCheckTime = now;

			ScopedMutex lock(resourceMutex);
			for (auto& res : resources)
			{
				if (res->GetReference() <= 0)
					toRemoved.push_back(res);
			}

			for (auto res : toRemoved)
			{
				if (res->GetReference() <= 0)
					ResourceManager::UnloadResoruce(res);
			}
			toRemoved.clear();
		}

		// Update resources cache
		cache.Save();
	}

	void ResourceManagerServiceImpl::Uninit()
	{
		// Mark is exit
		isExit = true;

		// Save resources cache
		cache.Save();

		// Uninit resource loading
		ContentLoadingManager::Uninitialize();

		// Flush pending objects
		ObjectService::FlushNow();

		// Unload remaining resources
		{
			ScopedMutex lock(resourceMutex);
			Array<Resource*> toDeleteResources;
			for (auto res : resources)
				toDeleteResources.push_back(res);

			for (auto res : toDeleteResources)
				res->DeleteObjectNow();
		}

		// Flush pending objects
		ObjectService::FlushNow();

		initialized = false;
	}
}