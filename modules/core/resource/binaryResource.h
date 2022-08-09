#pragma once

#include "resource.h"
#include "resourceStorage.h"
#include "resourceManager.h"

namespace VulkanTest
{
	typedef U16 AssetChunksFlag;

	class VULKAN_TEST_API BinaryResourceFactory : public ResourceFactory
	{
	protected:
		Resource* LoadResource(const Path& path)override;
	};

	class VULKAN_TEST_API BinaryResource : public Resource
	{
	public:
		virtual ~BinaryResource();

		bool InitStorage(const ResourceStorageRef& storage_);
		DataChunk* GetChunk(I32 index = 0)const;

		bool HasChunkLoaded(I32 index) const
		{
			ASSERT(index >= 0 && index < MAX_RESOURCE_DATA_CHUNKS);
			return header.chunks[index] != nullptr && header.chunks[index]->IsLoaded();
		}

	protected:
		BinaryResource(const Path& path_, ResourceFactory& resFactory_);
	
		void DoLoad() override;
		void DoUnload() override;
		void OnResourceLoaded(bool success);

	protected:
		virtual AssetChunksFlag GetChunksToPreload() const {
			return GET_CHUNK_FLAG(0);
		}

	private:
		ResourceStorageRef storage;
		ResourceChunkHeader header;
	};
}