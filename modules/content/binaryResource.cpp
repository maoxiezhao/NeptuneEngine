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

			// Load resource storage
			if (!storage->Load())
				return false;

			// Load resource init data from storage
			ResourceInitData initData;
			if (!storage->LoadResourceHeader(initData))
			{
				Logger::Warning("Failed to Load resource header");
				return false;
			}

			// Initialize resource
			if (!res->Initialize(initData))
			{
				Logger::Warning("Failed to initialize resource");
				return false;
			}
	
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

	class LoadChunkDataTask : public ContentLoadingTask
	{
	public:
		LoadChunkDataTask(BinaryResource* resource_, AssetChunksFlag chunkFlag_) :
			ContentLoadingTask(ContentLoadingTask::LoadResourceData),
			resource(resource_),
			chunkFlag(chunkFlag_),
			lock(resource_->storage->Lock())
		{
		}

		bool Run()override
		{
			ResPtr<BinaryResource> res = resource.get();
			if (res == nullptr)
				return false;

			ResourceStorage* storage = res->storage;
			if (!storage->IsLoaded())
				return false;

			for (int i = 0; i < MAX_RESOURCE_DATA_CHUNKS; i++)
			{
				if ((1 << i) & chunkFlag)
				{
					const auto chunk = res->GetChunk(i);
					if (chunk == nullptr)
						continue;

					if (!storage->LoadChunk(chunk))
						return false;
				}
			}

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
		AssetChunksFlag chunkFlag;
	};

	BinaryResource::BinaryResource(const ResourceInfo& info) :
		Resource(info),
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

	bool BinaryResource::Initialize(ResourceInitData& initData)
	{
		ASSERT(storage && storage->IsLoaded());
		header = initData.header;
		resSize = storage->Size();
		return Init(initData);
	}

	ContentLoadingTask* BinaryResource::CreateLoadingTask()
	{
		// Task pipeline
		// ----------------------------------------------------------
		// Load storage file -> Preload data chunks -> LoadResource
		// ----------------------------------------------------------
		auto task = Resource::CreateLoadingTask();
		ASSERT(task);

		// Preload chunks
		auto chunksToPreload = GetChunksToPreload();
		if (chunksToPreload > 0)
		{
			LoadChunkDataTask* loadTask = CJING_NEW(LoadChunkDataTask)(this, chunksToPreload);
			loadTask->SetNextTask(task);
			task = loadTask;
		}

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
		ResourceInitData initData;
		if (!storage->LoadResourceHeader(initData))
		{
			Logger::Warning("Failed to Load resource header");
			return;
		}

		// Reinitialize resource
		if (!Initialize(initData))
		{
			Logger::Error("Failed to reload resource storage %s", GetPath().c_str());
			return;
		}

		// Reload resource
		Reload();
	}

	bool BinaryResource::Initialize(ResourceHeader header_, const ResourceStorageRef& storage_)
	{
		ASSERT(!storage && IsEmpty());
		if (storage_ == storageRef)
			return true;

		storageRef = storage_;
		storage = storageRef.Get();
		header = header_;

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

	void BinaryResource::GetChunkData(I32 index, OutputMemoryStream& data)const
	{
		if (!HasChunkLoaded(index))
			return;

		DataChunk* chunk = GetChunk(index);
		data.Link(chunk->Data(), chunk->Size());
	}

	Task* BinaryResource::RequestChunkData(I32 index)
	{
		DataChunk* chunk = GetChunk(index);
		if (chunk != nullptr && chunk->IsLoaded())
		{
			chunk->RegisterUsage();
			return nullptr;
		}

		return CJING_NEW(LoadChunkDataTask)(this, GET_CHUNK_FLAG(index));
	}
}