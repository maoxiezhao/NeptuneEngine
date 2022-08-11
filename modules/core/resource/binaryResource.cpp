#include "binaryResource.h"

namespace VulkanTest
{
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
			resources[path.GetHashValue()] = res;
		
			auto storage = GetResourceManager().GetStorage(path);
			if (!static_cast<BinaryResource*>(res)->SetStorage(storage))
			{
				Logger::Warning("Cannot initialize resource %s", path.c_str());
				DestroyResource(res);
				return nullptr;
			}
		}

		return ResourceFactory::LoadResource(res);
	}

	BinaryResource::BinaryResource(const Path& path_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_),
		header(),
		storage(nullptr)
	{
	}

	BinaryResource::~BinaryResource()
	{
	}

	void BinaryResource::DoUnload()
	{
		if (storage)
			storage->Unload();

		Resource::DoUnload();
	}

	bool BinaryResource::InitStorage()
	{
		ASSERT(storage && storage->IsLoaded());
		ASSERT(GetState() != State::READY);
		ASSERT(emptyDepCount == 1);

		// Desired state changed when file loading, so is a invalid loading
		if (desiredState != State::READY)
			return false;

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

		// Load real resource
		if (!OnLoaded())
		{
			Logger::Error("Failed to load resource %s", GetPath().c_str());
			return false;
		}
		resSize = storage->Size();
		return true;
	}

	class LoadStorageTask : public ContentLoadingTask
	{
	public:
		LoadStorageTask(BinaryResource* resource_) :
			ContentLoadingTask(ContentLoadingTask::LoadResource),
			resource(resource_)
		{
		}

		bool Run()override
		{
			// TODO resoruce could be unloaded when task is running
			// Check resource state
			ASSERT(resource && resource->IsEmpty());

			ResourceStorage* storage = resource->storage;
			if (storage->IsLoaded())
				return false;

			if (!storage->Load())
				return false;
			
			if (!resource->InitStorage())
				return false;

			return true;
		}

		void OnEnd()override
		{
			Task::OnEnd();

			if (resource)
				resource->OnContentLoaded(GetState());
		}

	private:
		BinaryResource* resource;
	};

	ContentLoadingTask* BinaryResource::CreateLoadingTask()
	{
		LoadStorageTask* task = CJING_NEW(LoadStorageTask)(this);
		return task;
	}

	bool BinaryResource::SetStorage(const ResourceStorageRef& storage_)
	{
		ASSERT(!storage && IsEmpty());
		storageRef = storage_;
		storage = storageRef.get();
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