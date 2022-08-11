#pragma once

#include "resource.h"
#include "core\common.h"
#include "core\utils\path.h"
#include "core\utils\dataChunk.h"
#include "core\utils\intrusivePtr.hpp"
#include "core\platform\atomic.h"

namespace VulkanTest
{
#define INVALID_CHUNK_INDEX (-1)
#define MAX_RESOURCE_DATA_CHUNKS 16
#define GET_CHUNK_FLAG(chunkIndex) (1 << chunkIndex)

	class ResourceManager;

#pragma pack(1)
	struct VULKAN_TEST_API ResourceStorageHeader
	{
		static constexpr U32 MAGIC = 'FACK';
		static constexpr U32 VERSION = 0x01;

		U32 magic = MAGIC;
		U32 version = 0;
		U32 assetsCount = 0;
		U32 chunksCount = 0;
	};
#pragma pack()

	struct ResourceChunkHeader
	{
		DataChunk* chunks[MAX_RESOURCE_DATA_CHUNKS];
	};

	struct ResourceInitData
	{
		Path path;
		ResourceType resType;
		ResourceChunkHeader header;

		ResourceInitData(const Path& path_, ResourceType type_) :
			path(path_),
			resType(type_)
		{
			memset(&header, 0, sizeof(header));
		}
	};

#ifdef CJING3D_EDITOR
	struct ResourceDataWriter
	{
		ResourceInitData data;
		Array<DataChunk> chunks;

		ResourceDataWriter(const Path& path_, ResourceType type_) : data(path_, type_) {}

		DataChunk* GetChunk(I32 index)
		{
			ASSERT(index >= 0 && index < MAX_RESOURCE_DATA_CHUNKS);
			DataChunk* chunk = data.header.chunks[index];
			if (chunk == nullptr)
			{
				chunk = &chunks.emplace();
				data.header.chunks[index] = chunk;
			}
			return chunk;
		}
	};
#endif

	class ResourceStorage;
	struct ResourceStorageDeleter
	{
		void operator()(ResourceStorage* res);
	};

	class VULKAN_TEST_API ResourceStorage : public Util::IntrusivePtrEnabled<ResourceStorage, ResourceStorageDeleter>
	{
	public:
		struct ResourceEntry
		{
			U64 pathHash;
			ResourceType type;
			U32 address;
		};

		struct ChunkHeader
		{
			I32 chunkIndex[MAX_RESOURCE_DATA_CHUNKS];
		};

		ResourceStorage(const Path& path_, ResourceManager& resManager_);
		virtual ~ResourceStorage();

		bool Load();
		void Unload();
		void Tick();
		bool LoadChunksHeader(ResourceChunkHeader* resChunks);
		bool LoadChunk(DataChunk* chunk);
		bool ShouldDispose()const;
		bool Reload();

		bool IsLoaded() const {
			return isLoaded;
		}

		const Path& GetPath()const {
			return path;
		}

		U64 Size()const {
			return stream ? stream->Size() : 0;
		}

		void LockChunks() {
			AtomicIncrement(&chunksLock);
		}

		void UnlockChunks() {
			AtomicDecrement(&chunksLock);
		}

		// Scoed locker
		struct ScopedLock
		{
			friend ResourceStorage;
			static ScopedLock Invalid;

		private:
			ResourceStorage* storage;

			ScopedLock(ResourceStorage* storage_) :
				storage(storage_)
			{
				if (storage)
					storage->LockChunks();
			}

			~ScopedLock()
			{
				if (storage)
					storage->UnlockChunks();
			}
		};

#ifdef CJING3D_EDITOR
		static bool Save(OutputMemoryStream& output, const ResourceInitData& data);
#endif

	private:
		friend struct ResourceStorageDeleter;

		OutputMemoryStream* LoadContent();
		void CloseContent();

		Path path;
		ResourceManager& resManager;
		ResourceEntry entry;
		Array<DataChunk*> chunks;
		OutputMemoryStream* stream;
		bool isLoaded = false;
		Mutex mutex;
		volatile I64 chunksLock;
	};
	using ResourceStorageRef = Util::IntrusivePtr<ResourceStorage>;
}