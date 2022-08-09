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
		}

		auto storage = GetResourceManager().GetStorage(path);
		if (!static_cast<BinaryResource*>(res)->InitStorage(storage))
		{
			Logger::Warning("Cannot initialize resource %s", path.c_str());
			DestroyResource(res);
			return nullptr;
		}

		return ResourceFactory::LoadResource(res);
	}

	BinaryResource::BinaryResource(const Path& path_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_)
	{
	}

	BinaryResource::~BinaryResource()
	{
	}

	void BinaryResource::DoLoad()
	{
		ASSERT(storage);
		if (desiredState == State::READY)
			return;

		desiredState = State::READY;

		if (storage->IsLoaded())
		{
			OnResourceLoaded(true);
			return;
		}

		storage->GetLoadedCallback().Bind<&BinaryResource::OnResourceLoaded>(this);
		storage->Load();
	}

	void BinaryResource::DoUnload()
	{
		if (storage)
		{
			storage->GetLoadedCallback().Unbind<&BinaryResource::OnResourceLoaded>(this);
			storage.reset();
		}
		Resource::DoUnload();
	}

	void BinaryResource::OnResourceLoaded(bool success)
	{
		ASSERT(storage && storage->IsLoaded());

		// Desired state changed when file loading, so is a invalid loading
		if (desiredState != State::READY)
			return;

		ASSERT(GetState() != State::READY);
		ASSERT(emptyDepCount == 1);

		if (success == false)
		{
			Logger::Error("Failed to load %s", GetPath().c_str());
			emptyDepCount--;
			failedDepCount++;
			CheckState();
			return;
		}

		// TODO
		// Use jobsystem
// 
		// Load resource chunks
		if (!storage->LoadChunksHeader(&header))
		{
			failedDepCount++;
			goto CHECK_STATE;
		}

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
					{
						failedDepCount++;
						goto CHECK_STATE;
					}
				}
			}
		}

		// Load real resource
		if (!OnLoaded())
		{
			Logger::Error("Failed to load resource %s", GetPath().c_str());
			failedDepCount++;
		}
		resSize = storage->Size();

	CHECK_STATE:
		ASSERT(emptyDepCount > 0);
		emptyDepCount--;
		CheckState();
	}

	bool BinaryResource::InitStorage(const ResourceStorageRef& storage_)
	{
		ASSERT(!storage && IsEmpty());
		storage = storage_;
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