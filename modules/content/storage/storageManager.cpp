#include "storageManager.h"
#include "resourceManager.h"
#include "core\engine.h"

namespace VulkanTest
{
	class StorageServiceImpl : public EngineService
	{
	public:
		HashMap<U64, ResourceStorage*> storageMap;
		Array<std::pair<U64, ResourceStorage*>> toRemoved;
		Mutex mutex;

	public:
		StorageServiceImpl() :
			EngineService("StorageServiceImpl", -999)
		{}

		bool Init(Engine& engine) override
		{
			initialized = true;
			return true;
		}

		void LateUpdate() override
		{
			ScopedMutex lock(mutex);

			// Process storages
			// Release resource storage if it should dispose (no reference and no lock)
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

		void Uninit() override
		{
			ScopedMutex lock(mutex);
			for (auto storage : storageMap)
			{
				if (storage != nullptr)
				{
					storage->Unload();
					CJING_SAFE_DELETE(storage);
				}
			}
			storageMap.clear();
			initialized = false;
		}
	};
	StorageServiceImpl StorageServiceImplInstance;

	ResourceStorageRef StorageManager::TryGetStorage(const Path& path, bool isCompiled)
	{
		auto& impl = StorageServiceImplInstance;
		ScopedMutex lock(impl.mutex);
		ResourceStorage* storage = nullptr;
		Path contentPath = ResourceStorage::GetContentPath(path, isCompiled);
		impl.storageMap.tryGet(contentPath.GetHashValue(), storage);
		return storage;
	}

	ResourceStorageRef StorageManager::GetStorage(const Path& path, ResourceManager& resManager, bool doLoad, bool isCompiled)
	{
		auto& impl = StorageServiceImplInstance;
		impl.mutex.Lock();
		Path contentPath = ResourceStorage::GetContentPath(path, isCompiled);
		ResourceStorage* ret = nullptr;
		auto it = impl.storageMap.find(contentPath.GetHashValue());
		if (!it.isValid())
		{
			auto newStorage = CJING_NEW(ResourceStorage)(path, isCompiled, resManager);
			impl.storageMap.insert(contentPath.GetHashValue(), newStorage);
			ret = newStorage;
		}
		else
		{
			ret = it.value();
		}
		impl.mutex.Unlock();

		if (doLoad)
		{
			ret->LockChunks();
			const bool loadRet = ret->Load();
			ret->UnlockChunks();
			if (loadRet == false)
			{
				Logger::Error("Failed to load storage %s", path.c_str());

				ScopedMutex lock(impl.mutex);
				impl.storageMap.erase(contentPath.GetHashValue());
				CJING_DELETE(ret);
				return nullptr;
			}
		}

		return ResourceStorageRef(ret);
	}
}