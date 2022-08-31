#include "storageManager.h"
#include "resourceManager.h"
#include "engine.h"

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

	ResourceStorageRef StorageManager::GetStorage(const Path& path, ResourceManager& resManager)
	{
		auto& impl = StorageServiceImplInstance;
		ScopedMutex lock(impl.mutex);
		ResourceStorage* ret = nullptr;
		auto it = impl.storageMap.find(path.GetHashValue());
		if (!it.isValid())
		{
			auto newStorage = CJING_NEW(ResourceStorage)(path, resManager);
			impl.storageMap.insert(path.GetHashValue(), newStorage);
			ret = newStorage;
		}
		else
		{
			ret = it.value();
		}
		return ResourceStorageRef(ret);
	}
}