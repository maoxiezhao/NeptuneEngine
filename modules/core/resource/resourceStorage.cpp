#include "resourceStorage.h"
#include "compress\compressor.h"
#include "core\platform\timer.h"

namespace VulkanTest
{
	constexpr U32 COMPRESSION_SIZE_LIMIT = 4096;

	ResourceStorage::ScopedLock ResourceStorage::ScopedLock::Invalid(nullptr);

	void ResourceStorageDeleter::operator()(ResourceStorage* res)
	{
		res->Unload();
	}

	ResourceStorage::ResourceStorage(const Path& path_, FileSystem& fs_) :
		path(path_),
		fs(fs_),
		asyncHandle(AsyncLoadHandle::INVALID),
		chunksLock(0)
	{
	}

	ResourceStorage::~ResourceStorage()
	{
		ASSERT(!IsLoaded());
		ASSERT(chunksLock == 0);
	}

	void ResourceStorage::Load()
	{
		if (isLoaded)
			return;

		if (asyncHandle.IsValid())
			return;

		AsyncLoadCallback cb;
		cb.Bind<&ResourceStorage::OnFileLoaded>(this);

		// Load resource tiles directly
		if (StartsWith(GetPath().c_str(), ".export/resources_tiles/"))
		{
			asyncHandle = fs.LoadFileAsync(GetPath(), cb);
			return;
		}

		// Load normal compiled resources
		const U64 pathHash = path.GetHashValue();
		StaticString<MAX_PATH_LENGTH> fullResPath(".export/resources/", pathHash, ".res");
		asyncHandle = fs.LoadFileAsync(Path(fullResPath), cb);
	}

	void ResourceStorage::Unload()
	{
		if (!isLoaded)
			return;

		if (asyncHandle.IsValid())
		{
			// Cancel when res is loading
			fs.CancelAsync(asyncHandle);
			asyncHandle = AsyncLoadHandle::INVALID;
		}

		I32 waitTime = 10;
		while (AtomicRead(&chunksLock) != 0 && waitTime-- > 0)
			Platform::Sleep(10);

		if (AtomicRead(&chunksLock) != 0)
		{
			// TODO
			ASSERT(false);
		}

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
			buffer.Free();
	}

	bool ResourceStorage::LoadChunksHeader(ResourceChunkHeader* resChunks)
	{
		ASSERT(isLoaded);
		if (buffer.Size() == 0)
			return false;

		InputMemoryStream input(buffer);
		input.SetPos(entry.address);

		ChunkHeader chunkHeader;
		input.Read(chunkHeader);
		for (int i = 0; i < ARRAYSIZE(chunkHeader.chunkIndex); i++)
		{
			I32 chunkIndex = chunkHeader.chunkIndex[i];
			if (chunkIndex >= (I32)chunks.size())
			{
				Logger::Warning("Invalid chunk mapping");
				return false;
			}
			resChunks->chunks[i] = chunkIndex == INVALID_CHUNK_INDEX ? nullptr : chunks[i];
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

		if (buffer.Size() == 0)
			return false;

		ScopedLock lock(this);
		InputMemoryStream input(buffer);
		input.SetPos(chunk->location.Address);

		auto size = chunk->location.Size;
		if (chunk->compressed)
		{
			size -= sizeof(I32);
			I32 originalSize;
			input.Read(originalSize);

			OutputMemoryStream tmp;
			tmp.Resize(originalSize);
			I32 decompressedSize = Compressor::Decompress(
				(const char*)input.GetBuffer() + input.GetPos(),
				(char*)tmp.Data(),
				I32(size),
				I32(tmp.Size()));

			if (decompressedSize != originalSize)
				return false;

			chunk->mem.Write(tmp.Data(), tmp.Size());
		}
		else
		{
			chunk->mem.Resize(size);
			input.Read(chunk->mem.Data(), size);
		}

		chunk->RegisterUsage();
		return true;
	}

	bool ResourceStorage::ShouldDispose() const
	{
		return GetReference() == 0 && AtomicRead((I64*)&chunksLock) == 0;
	}

	bool ResourceStorage::Save(OutputMemoryStream& output, const ResourceInitData& data)
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
		// Chunk header (count == entries count)
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
		entry.pathHash = data.path.GetHashValue();
		entry.type = data.resType;
		entry.address = currentAddress;
		output.Write(entry);

		currentAddress += sizeof(ChunkHeader);

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

				if (compressedSize <= 0 || compressedSize > I32(chunk->Size() / 4 * 3))
				{
					compressedChunks.clear();
					compressed = false;
				}
	
				chunk->compressed = compressed;
			}
		}

		// Write chunk locations
		for (U32 i = 0; i < header.chunksCount; i++)
		{
			I32 size = chunks[i]->Size();
			if (compressedChunks[i].Size() > 0)
				size = compressedChunks[i].Size() + sizeof(I32); // Add original data size
			chunks[i]->location.Size = size;
			chunks[i]->location.Address = currentAddress;

			currentAddress += size;

			output.Write(chunks[i]->location);
			output.Write(chunks[i]->compressed);
		}

		// Write chunk header
		ChunkHeader chunkHeader;
		for (U32 i = 0; i < MAX_RESOURCE_DATA_CHUNKS; i++)
			chunkHeader.chunkIndex[i] = chunks.indexOf(data.header.chunks[i]);
		output.Write(chunkHeader);

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

	void ResourceStorage::OnFileLoaded(U64 size, const U8* mem, bool success)
	{
		ASSERT(asyncHandle.IsValid());
		asyncHandle = AsyncLoadHandle::INVALID;
		if (!success)
		{
			cb.Invoke(false);
			return;
		}

		if (size < sizeof(ResourceStorageHeader))
		{
			Logger::Warning("Invalid compiled resource %s", GetPath().c_str());
			success = false;
			asyncHandle = AsyncLoadHandle::INVALID;
			cb.Invoke(success);
			return;
		}

		// Resource format
		// -----------------------------------
		// ResourceStorageHeader
		// Entries
		// Chunk locations
		// Chunk header (count == entries count)
		// Chunk datas

		// ResourceStorageHeader
		InputMemoryStream input(mem + sizeof(ResourceStorageHeader), size - sizeof(ResourceStorageHeader));
		const ResourceStorageHeader* resHeader = (const ResourceStorageHeader*)mem;
		if (resHeader->magic != ResourceStorageHeader::MAGIC)
		{
			Logger::Warning("Invalid compiled resource %s", GetPath().c_str());
			success = false;
			goto CHECK_STATE;
		}

		if (resHeader->version != ResourceStorageHeader::VERSION)
		{
			Logger::Warning("Invalid compiled resource %s", GetPath().c_str());
			success = false;
			goto CHECK_STATE;
		}

		// Entries
		ASSERT_MSG(resHeader->assetsCount == 1, "Unsupport multiple resources now");
		for (U32 i = 0; i < resHeader->assetsCount; i++)
		{
			ResourceEntry entry;
			input.Read(entry);
			this->entry = entry;
		}

		// Chunk locations
		for (U32 i = 0; i < resHeader->chunksCount; i++)
		{
			DataChunk::Location location;
			input.Read(location);
			if (location.Size == 0)
			{
				Logger::Warning("Empty chunk %s", GetPath().c_str());
				success = false;
				goto CHECK_STATE;
			}

			auto chunk = CJING_NEW(DataChunk);
			chunk->location = location;
			input.Read(chunk->compressed);
			chunks.push_back(chunk);
		}

		buffer.Resize(size);
		memcpy(buffer.Data(), mem, size);
		isLoaded = true;

	CHECK_STATE:
		asyncHandle = AsyncLoadHandle::INVALID;
		cb.Invoke(success);
	}
}