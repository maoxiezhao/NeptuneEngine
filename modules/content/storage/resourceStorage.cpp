#include "resourceStorage.h"
#include "resourceManager.h"
#include "core\serialization\fileWriteStream.h"
#include "compress\compressor.h"

namespace VulkanTest
{
	constexpr U32 COMPRESSION_SIZE_LIMIT = 4096;

	struct VULKAN_TEST_API ResourceStorageHeader
	{
		static constexpr U32 MAGIC = 'FACK';
		static constexpr U32 VERSION = 0x01;

		U32 magic = MAGIC;
		U32 version = 0;
		U32 assetsCount = 0;
		U32 chunksCount = 0;
	};

	ResourceStorage::StorageLock ResourceStorage::StorageLock::Invalid(nullptr);

	ResourceStorage::ResourceStorage(const Path& path_) :
		path(path_),
		chunksLock(0),
		refCount(0),
		lastRefLoseTime(0.0f)
	{
	}

	ResourceStorage::~ResourceStorage()
	{
		ASSERT(!IsLoaded());
		ASSERT(chunksLock == 0);
		// ASSERT(refCount == 0);
	}

	bool ResourceStorage::Load()
	{
		if (IsLoaded())
			return true;

		ScopedMutex lock(mutex);
		if (IsLoaded())
			return true;

		FileReadStream* inputMem = LoadContent();
		if (inputMem == nullptr)
			return false;

		// Resource format
		// -----------------------------------
		// ResourceStorageHeader
		// Entries
		// Chunk locations
		// resource header (count == entries count)
		// Chunk datas

		// ResourceStorageHeader
		ResourceStorageHeader resHeader;
		inputMem->Read(resHeader);
		if (resHeader.magic != ResourceStorageHeader::MAGIC)
		{
			Logger::Warning("Invalid compiled resource %s", GetPath().c_str());
			return false;
		}

		if (resHeader.version != ResourceStorageHeader::VERSION)
		{
			Logger::Warning("Invalid compiled resource %s", GetPath().c_str());
			return false;
		}

		// Entries
		ASSERT_MSG(resHeader.assetsCount == 1, "Unsupport multiple resources now");
		for (U32 i = 0; i < resHeader.assetsCount; i++)
		{
			ResourceEntry entry;
			inputMem->Read(entry);
			this->entry = entry;
		}

		// Chunk locations
		for (U32 i = 0; i < resHeader.chunksCount; i++)
		{
			DataChunk::Location location;
			inputMem->Read(location);
			if (location.Size == 0)
			{
				Logger::Warning("Empty chunk %s", GetPath().c_str());
				return false;
			}

			auto chunk = CJING_NEW(DataChunk);
			chunk->location = location;
			inputMem->Read(chunk->compressed);
			chunks.push_back(chunk);
		}

		isLoaded = true;
		return true;
	}

	void ResourceStorage::Unload()
	{
		if (!isLoaded)
			return;

		CloseContent();

		for (auto chunk : chunks)
			CJING_SAFE_DELETE(chunk);
		chunks.clear();

		isLoaded = false;
	}

	void ResourceStorage::Tick()
	{
		// Chunks are locked
		if (AtomicRead(&chunksLock) != 0)
			return;

		const static U64 UnusedDataChunksLifetime = 10 * Timer::GetFrequency();

		bool wasAnyUsed = false;
		const auto now = Timer::GetRawTimestamp();
		for (U32 i = 0; i < chunks.size(); i++)
		{
			auto chunk = chunks[i];
			bool used = (now - AtomicRead((I64*)&chunk->LastAccessTime)) < UnusedDataChunksLifetime;
			if (!used && chunk->IsLoaded())
				chunk->Unload();

			wasAnyUsed |= used;
		}

		// Clear file buffer, if all chunks are unused;
		if (!wasAnyUsed && AtomicRead(&chunksLock) == 0)
			CloseContent();
	}

	bool ResourceStorage::LoadResourceHeader(ResourceInitData& initData)
	{
		ASSERT(isLoaded);

		FileReadStream* input = LoadContent();
		if (input == nullptr)
			return false;

		input->SetPos(entry.address);

		// ReasourceHeader
		// ----------------------------------
		// Guid
		// Type
		// Chunk mapping
		// Custom data

		// Guid
		input->Read(initData.header.guid);

		// Type
		U64 hash = input->Read<U64>();
		initData.header.type = ResourceType(hash);
		if (initData.header.type == ResourceType::INVALID_TYPE)
		{
			Logger::Warning("Invalid resource data stream");
			return false;
		}

		// Chunk mapping
		ChunkMapping chunkMapping;
		input->Read(chunkMapping);
		for (int i = 0; i < MAX_RESOURCE_DATA_CHUNKS; i++)
		{
			I32 chunkIndex = chunkMapping.chunkIndex[i];
			if (chunkIndex >= (I32)chunks.size())
			{
				Logger::Warning("Invalid chunk mapping");
				return false;
			}
			initData.header.chunks[i] = chunkIndex == INVALID_CHUNK_INDEX ? nullptr : chunks[chunkIndex];
		}

		// Custom data
		I32 size = input->Read<I32>();
		if (size > 0)
		{
			initData.customData.Resize(size);
			input->Read(initData.customData.Data(), size);
		}

		return true;
	}

	bool ResourceStorage::LoadChunk(DataChunk* chunk)
	{
		ASSERT(isLoaded);
		ASSERT(chunk != nullptr && chunks.indexOf(chunk) != -1);

		if (chunk->IsLoaded())
			return true;

		if (!chunk->ExistsInFile())
		{
			Logger::Warning("Invalid chunk");
			return false;
		}

		FileReadStream* input = LoadContent();
		if (input == nullptr)
			return false;

		StorageLock lock(this);
		input->SetPos(chunk->location.Address);

		auto size = chunk->location.Size;
		if (chunk->compressed)
		{
			size -= sizeof(I32);
			I32 originalSize;
			input->Read(originalSize);

			OutputMemoryStream tmp;
			tmp.Resize(size);
			input->Read(tmp.Data(), size);

			chunk->mem.Allocate((U64)originalSize);
			I32 decompressedSize = Compressor::Decompress(
				(char*)tmp.Data(),
				(char*)chunk->mem.Data() + chunk->mem.Size(),
				I32(size),
				I32(originalSize));

			if (decompressedSize != originalSize)
				return false;

			chunk->mem.Resize(decompressedSize);
		}
		else
		{
			chunk->mem.Resize(size);
			input->Read(chunk->mem.Data(), size);
		}

		chunk->RegisterUsage();
		return true;
	}

	DataChunk* ResourceStorage::AllocateChunk()
	{
		auto chunk = CJING_NEW(DataChunk);
		chunks.push_back(chunk);
		return chunk;
	}

	bool ResourceStorage::ShouldDispose() const
	{
		F32 refLostTime = ((F32)Timer::GetRawTimestamp() / (F32)Timer::GetFrequency());
		return GetReference() == 0 && 
			AtomicRead((I64*)&chunksLock) == 0 &&
			refLostTime - lastRefLoseTime >= 0.5f;
	}

	bool ResourceStorage::Reload()
	{
		if (!IsLoaded())
		{
			Logger::Warning("Resoure isn't loaded %s", GetPath().c_str());
			return false;
		}

		Unload();
		bool ret = Load();
		OnReloaded.Invoke(this, ret);
		return ret;
	}

	U64 ResourceStorage::Size()
	{
		auto stream = LoadContent();
		return stream->Size();
	}

	I32 ResourceStorage::GetReference() const
	{
		return (I32)AtomicRead(const_cast<I64 volatile*>(&refCount));
	}

#ifdef CJING3D_EDITOR
	
	bool ResourceStorage::Create(const Path& path, const ResourceInitData& initData)
	{
		auto storage = StorageManager::EnsureAccess(path);
		auto stream = FileWriteStream::Open(path);
		if (stream == nullptr)
			return false;

		bool ret = Save(*stream, initData);

		CJING_DELETE(stream);

		// Reload storage if is loaded
		if (storage)
			storage->Reload();

		return ret;
	}

	bool ResourceStorage::Save(IOutputStream& output, const ResourceInitData& data)
	{
		Array<DataChunk*> chunks;
		for (I32 i = 0; i < MAX_RESOURCE_DATA_CHUNKS; i++)
		{
			if (data.header.chunks[i] != nullptr && data.header.chunks[i]->IsLoaded())
				chunks.push_back(data.header.chunks[i]);
		}

		// Resource format
		// -----------------------------------
		// ResourceStorageHeader
		// Entries
		// Chunk locations
		// Chunk mapping (count == entries count)
		// Chunk datas

		// Write header
		ResourceStorageHeader header;
		header.magic = ResourceStorageHeader::MAGIC;
		header.version = ResourceStorageHeader::VERSION;
		header.assetsCount = 1;
		header.chunksCount = chunks.size();
		output.Write(header);

		// Write entry
		U32 currentAddress = sizeof(header) + sizeof(entry) + (sizeof(DataChunk::location) + sizeof(U8)) * header.chunksCount;

		ResourceStorage::ResourceEntry entry;
		entry.guid = data.header.guid;
		entry.type = data.header.type;
		entry.address = currentAddress;
		output.Write(entry);

		// Entry address -> ReasourceHeader(Guid, ResourceType, chunkMapping)
		currentAddress += 
			sizeof(Guid) +								// GUID
			sizeof(U64) +								// TypeName
			sizeof(ChunkMapping) +						// ChunkMapping
			sizeof(I32) + (U32)data.customData.Size();  // Custom data size + data

		// Compress chunk
		Array<OutputMemoryStream> compressedChunks;
		compressedChunks.resize(header.chunksCount);
		for (U32 i = 0; i < header.chunksCount; i++)
		{
			auto chunk = chunks[i];
			bool compressed = chunk->compressed;
			if (!compressed && chunk->Size() > COMPRESSION_SIZE_LIMIT)
				compressed = true;

			if (compressed)
			{
				U64 compressedSize;
				if (!Compressor::Compress(compressedChunks[i], compressedSize, chunk->mem))
				{
					Logger::Error("Could not compress chunk");
					return false;
				}

				// TODO
				//if (compressedSize <= 0 || compressedSize > I32(chunk->Size() / 4 * 3))
				//{
				//	compressedChunks.clear();
				//	compressed = false;
				//}
	
				chunk->compressed = compressed;
			}
		}

		// Write chunk locations
		for (U32 i = 0; i < header.chunksCount; i++)
		{
			U64 size = chunks[i]->Size();
			if (compressedChunks[i].Size() > 0)
				size = compressedChunks[i].Size() + sizeof(I32); // Add original data size
			chunks[i]->location.Size = (U32)size;
			chunks[i]->location.Address = currentAddress;

			currentAddress += (U32)size;

			output.Write(chunks[i]->location);
			output.Write(chunks[i]->compressed);
		}

		// Write resource header
		// ---------------------------------------
		// Guid
		// Type
		// Chunk mapping
		// Custom data

		output.Write(data.header.guid);
		output.Write(data.header.type.GetHashValue());

		ChunkMapping chunkMapping;
		for (U32 i = 0; i < MAX_RESOURCE_DATA_CHUNKS; i++)
			chunkMapping.chunkIndex[i] = chunks.indexOf(data.header.chunks[i]);
		output.Write(chunkMapping);

		output.Write((I32)data.customData.Size());
		if (data.customData.Size() > 0)
			output.Write(data.customData.Data(), data.customData.Size());

		// Write chunk data
		for (U32 i = 0; i < header.chunksCount; i++)
		{
			if (compressedChunks[i].Size() > 0)
			{
				output.Write((I32)chunks[i]->Size());
				output.Write(compressedChunks[i].Data(), compressedChunks[i].Size());
			}
			else
			{
				output.Write(chunks[i]->Data(), chunks[i]->Size());
			}
		}

		return true;
	}
#endif

	FileReadStream* ResourceStorage::LoadContent()
	{
		FileReadStream*& stream = file.Get();
		if (stream == nullptr)
		{
			if (AtomicRead(&chunksLock) != 0)
				int a = 0;

			auto file_ = FileSystem::OpenFile(path.c_str(), FileFlags::DEFAULT_READ);
			if (!file_ || !file_->IsValid())
			{
				Logger::Error("Cannot open compiled resource content %s", path.c_str());
				return nullptr;
			}

			stream = CJING_NEW(FileReadStream)(std::move(file_));
		}
		return stream;
	}

	void ResourceStorage::CloseContent()
	{
		I32 waitTime = 10;
		while (AtomicRead(&chunksLock) != 0 && waitTime-- > 0)
			Platform::Sleep(10);

		if (AtomicRead(&chunksLock) != 0)
		{
			// Storage can be locked by some streaming tasks
			auto entry = GetResourceEntry();
			auto res = ResourceManager::GetResource(entry.guid);
			if (res != nullptr)
				res->CancelStreaming();
		}

		ASSERT(chunksLock == 0);
		file.DeleteAll();
	}
}