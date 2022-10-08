#pragma once

#include "resource.h"
#include "storage\resourceStorage.h"
#include "binaryResourceFactory.h"
#include "resourceManager.h"

namespace VulkanTest
{
	typedef U16 AssetChunksFlag;

	class VULKAN_TEST_API BinaryResource : public Resource
	{
	public:
		virtual ~BinaryResource();

		bool Initialize(ResourceHeader header_, const ResourceStorageRef& storage_);
		bool Initialize(ResourceInitData& initData);

		DataChunk* GetChunk(I32 index = 0)const;
		DataChunk* GetOrCreateChunk(I32 index = 0);
		void GetChunkData(I32 index, OutputMemoryStream& data)const;
		Task* RequestChunkData(I32 index);
		bool LoadChunks(AssetChunksFlag flags);
		void ReleaseChunk(I32 index);

		bool HasChunk(I32 index)const
		{
			ASSERT(index >= 0 && index < MAX_RESOURCE_DATA_CHUNKS);
			return header.chunks[index] != nullptr;
		}

		bool HasChunkLoaded(I32 index) const
		{
			ASSERT(index >= 0 && index < MAX_RESOURCE_DATA_CHUNKS);
			return header.chunks[index] != nullptr && header.chunks[index]->IsLoaded();
		}

	public:
		// Real resourceStorage pointer from storageRef
		ResourceStorage* storage;

	protected:
		friend class BinaryResourceFactoryBase;

		BinaryResource(const ResourceInfo& info);

		bool LoadResource() override;
		ContentLoadingTask* CreateLoadingTask()override;
		void OnStorageReloaded(ResourceStorage* storage_, bool ret);

		virtual bool Init(ResourceInitData& initData) {
			return true;
		}

		virtual AssetChunksFlag GetChunksToPreload()const {
			return GET_CHUNK_FLAG(0);
		}

	private:
		ResourceStorageRef storageRef;
		ResourceHeader header;
	};
}