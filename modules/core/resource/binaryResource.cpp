#include "binaryResource.h"

namespace VulkanTest
{
	class LoadStorageTask : public ContentLoadingTask
	{
	public:
		LoadStorageTask(BinaryResource* resource_) :
			ContentLoadingTask(ContentLoadingTask::LoadResource),
			resource(resource_),
			lock(resource_->storage->Lock())
		{
		}

		bool Run()override
		{
			ResPtr<BinaryResource> res = resource.get();
			if (res == nullptr)
				return false;

			ResourceStorage* storage = res->storage;
			if (storage->IsLoaded())
				return false;

			if (!storage->Load())
				return false;

			if (!res->InitStorage())
				return false;

			return true;
		}

		void OnEnd()override
		{
			lock.Release();
			Task::OnEnd();
		}

	private:
		WeakResPtr<BinaryResource> resource;
		ResourceStorage::StorageLock lock;
	};

	Resource* BinaryResourceFactory::LoadResource(const Path& path)
	{
		ASSERT(resManager != nullptr);

		if (path.IsEmpty())
			return nullptr;

		Resource* res = GetResource(path);
		if (res == nullptr)
		{
			res = static_cast<BinaryResource*>(CreateResource(path));
			if (res == nullptr)
			{
				Logger::Warning("Invalid binary resource %s", path.c_str());
				return nullptr;
			}
		
			auto storage = GetResourceManager().GetStorage(path);
			if (!static_cast<BinaryResource*>(res)->SetStorage(storage))
			{
				Logger::Warning("Cannot initialize resource %s", path.c_str());
				DestroyResource(res);
				return nullptr;
			}

			ScopedMutex lock(resLock);
			resources[path.GetHashValue()] = res;
		}

		return ResourceFactory::LoadResource(res);
	}

	void BinaryResourceFactory::ContinuleLoadResource(Resource* res)
	{
		BinaryResource* binaryRes = static_cast<BinaryResource*>(res);
		ASSERT(binaryRes);

		binaryRes->RemoveReference();
		binaryRes->SetHooked(false);
		binaryRes->desiredState = Resource::State::EMPTY;
		if (binaryRes->IsReloading())
		{
			binaryRes->storage->Reload();
		}
		else
		{
			binaryRes->DoLoad();
		}
	}

	void BinaryResourceFactory::ReloadResource(Resource* res)
	{
		ASSERT(res != nullptr);

		BinaryResource* binaryRes = static_cast<BinaryResource*>(res);
		// Load resource
		if (resManager->OnBeforeLoad(*binaryRes) == ResourceManager::LoadHook::Action::DEFERRED)
		{
			ASSERT(binaryRes->IsHooked() == false);
			binaryRes->SetIsReloading(true);
			binaryRes->SetHooked(true);
			binaryRes->AddReference(); // Hook
			binaryRes->desiredState = Resource::State::READY;
		}
		else
		{
			binaryRes->storage->Reload();
		}
	}

	BinaryResource::BinaryResource(const Path& path_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_),
		header(),
		storage(nullptr),
		storageRef(nullptr)
	{
	}

	BinaryResource::~BinaryResource()
	{
#ifdef CJING3D_EDITOR
		if (storage)
			storage->OnReloaded.Unbind<&BinaryResource::OnStorageReloaded>(this);
#endif
	}

	bool BinaryResource::LoadResource()
	{
		ASSERT(storage && storage->IsLoaded());
		auto lock = storage->Lock();
		return Load();
	}

	bool BinaryResource::InitStorage()
	{
		ASSERT(storage && storage->IsLoaded());

		// Load resource chunk header
		if (!storage->LoadChunksHeader(&header))
			return false;

		// Preload chunks
		if (GetChunksToPreload() > 0)
		{
			auto chunkFlags = GetChunksToPreload();
			for (int i = 0; i < MAX_RESOURCE_DATA_CHUNKS; i++)
			{
				if ((1 << i) & chunkFlags)
				{
					const auto chunk = GetChunk(i);
					if (chunk == nullptr)
						continue;

					if (!storage->LoadChunk(chunk))
						return false;
				}
			}
		}

		resSize = storage->Size();
		return true;
	}

	ContentLoadingTask* BinaryResource::CreateLoadingTask()
	{
		auto task = Resource::CreateLoadingTask();
		ASSERT(task);

		// Load resource storage first
		if (!storage->IsLoaded())
		{
			LoadStorageTask* loadTask = CJING_NEW(LoadStorageTask)(this);
			loadTask->SetNextTask(task);
			task = loadTask;
		}

		return task;
	}

	void BinaryResource::OnStorageReloaded(ResourceStorage* storage_, bool ret)
	{
		ASSERT(storage_ && storage == storage_);
		auto oldHeader = header;
		memset(&header, 0, sizeof(header));
		if (ret == false)
		{
			Logger::Error("Failed to reload resource storage %s", GetPath().c_str());
			return;
		}

		// Reinitialize storage
		if (!InitStorage())
		{
			Logger::Error("Failed to reload resource storage %s", GetPath().c_str());
			return;
		}

		// Reload resource
		Reload();
	}

	bool BinaryResource::SetStorage(const ResourceStorageRef& storage_)
	{
		ASSERT(!storage && IsEmpty());
		if (storage_ == storageRef)
			return true;

		storageRef = storage_;
		storage = storageRef.Get();

#ifdef CJING3D_EDITOR
		if (storage)
			storage->OnReloaded.Bind<&BinaryResource::OnStorageReloaded>(this);
#endif
		return true;
	}

	DataChunk* BinaryResource::GetChunk(I32 index) const
	{
		ASSERT(index >= 0 && index < MAX_RESOURCE_DATA_CHUNKS);
		auto chunk = header.chunks[index];
		if (chunk)
			chunk->RegisterUsage();
		return chunk;
	}

}