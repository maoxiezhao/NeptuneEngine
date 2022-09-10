#pragma once

#include "resource.h"
#include "storage\resourceStorage.h"
#include "resourceManager.h"

namespace VulkanTest
{
	typedef U16 AssetChunksFlag;

	class VULKAN_TEST_API BinaryResourceFactory : public ResourceFactory
	{
	protected:
		Resource* NewResource(const ResourceInfo& info) override;
		Resource* CreateTemporaryResource(const ResourceInfo& info) override;
	};

	class VULKAN_TEST_API BinaryResource : public Resource
	{
	public:
		virtual ~BinaryResource();

		bool Initialize(ResourceHeader header_, const ResourceStorageRef& storage_);
		bool Initialize(ResourceInitData& initData);

		DataChunk* GetChunk(I32 index = 0)const;
		void GetChunkData(I32 index, OutputMemoryStream& data)const;
		Task* RequestChunkData(I32 index);

		bool HasChunkLoaded(I32 index) const
		{
			ASSERT(index >= 0 && index < MAX_RESOURCE_DATA_CHUNKS);
			return header.chunks[index] != nullptr && header.chunks[index]->IsLoaded();
		}

		// Real resourceStorage pointer from storageRef
		ResourceStorage* storage;

	protected:
		friend class BinaryResourceFactory;

		BinaryResource(const ResourceInfo& info, ResourceManager& resManager_);

		bool LoadResource() override;
		ContentLoadingTask* CreateLoadingTask()override;

		virtual bool Init(ResourceInitData& initData) 
		{
			return true;
		}

		virtual AssetChunksFlag GetChunksToPreload() const 
		{
			return GET_CHUNK_FLAG(0);
		}

		void OnStorageReloaded(ResourceStorage* storage_, bool ret);

	private:
		ResourceStorageRef storageRef;
		ResourceHeader header;
	};
}