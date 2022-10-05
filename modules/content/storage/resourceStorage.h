#pragma once

#include "resource.h"
#include "resourceHeader.h"
#include "core\platform\timer.h"
#include "core\serialization\fileReadStream.h"
#include "core\utils\threadLocal.h"

namespace VulkanTest
{
	class ResourceManager;

	class VULKAN_TEST_API ResourceStorage : public Object
	{
	public:
		struct ResourceEntry
		{
			Guid guid;
			ResourceType type;
			U32 address;
		};

		struct ChunkMapping
		{
			I32 chunkIndex[MAX_RESOURCE_DATA_CHUNKS];
		};

		ResourceStorage(const Path& path_);
		virtual ~ResourceStorage();

		bool Load();
		void Unload();
		void Tick();
		bool LoadResourceHeader(ResourceInitData& initData);
		bool LoadChunk(DataChunk* chunk);
		bool ShouldDispose()const;
		bool Reload();
		U64 Size();
		void CloseContent();

		bool IsLoaded() const {
			return isLoaded;
		}

		const Path& GetPath()const {
			return path;
		}

		void LockChunks() {
			AtomicIncrement(&chunksLock);
		}

		void UnlockChunks() {
			AtomicDecrement(&chunksLock);
		}

		void AddReference() {
			AtomicIncrement(&refCount);
		}

		void RemoveReference() 
		{
			AtomicDecrement(&refCount);
			if (AtomicRead(&refCount) == 0)
				lastRefLoseTime = (F32)Timer::GetRawTimestamp() / (F32)Timer::GetFrequency();
		}

		ResourceEntry GetResourceEntry() {
			return entry;
		}

		I32 GetReference() const;

		// Scoed locker
		struct StorageLock
		{
			friend ResourceStorage;
			static StorageLock Invalid;

			StorageLock(const StorageLock& lock) :
				storage(lock.storage)
			{
				if (storage)
					storage->LockChunks();
			}

			StorageLock(StorageLock&& lock) noexcept :
				storage(lock.storage)
			{
				lock.storage = nullptr;
			}

			~StorageLock()
			{
				if (storage)
					storage->UnlockChunks();
			}

			void Release()
			{
				if (storage)
				{
					storage->UnlockChunks();
					storage = nullptr;
				}
			}

		private:
			ResourceStorage* storage;

			StorageLock(ResourceStorage* storage_) :
				storage(storage_)
			{
				if (storage)
					storage->LockChunks();
			}
		};

		StorageLock Lock()
		{
			return StorageLock(this);
		}

#ifdef CJING3D_EDITOR
		static bool Save(OutputMemoryStream& output, const ResourceInitData& data);

		DelegateList<void(ResourceStorage*, bool)> OnReloaded; 
#endif

	private:
		FileReadStream* LoadContent();

		Path path;
		ResourceEntry entry;
		Array<DataChunk*> chunks;
		ThreadLocalObject<FileReadStream> file;
		bool isLoaded = false;
		Mutex mutex;
		volatile I64 chunksLock;
		volatile I64 refCount;
		F32 lastRefLoseTime;
	};

	class VULKAN_TEST_API ResourceStorageRef
	{
	private:
		ResourceStorage* storage;

	public:
		ResourceStorageRef(ResourceStorage* storage_) :
			storage(storage_)
		{
			if (storage)
				storage->AddReference();
		}

		~ResourceStorageRef()
		{
			if (storage)
				storage->RemoveReference();
		}

		ResourceStorageRef(const ResourceStorageRef& other) :
			storage(other.storage)
		{
			if (storage)
				storage->AddReference();
		}

		ResourceStorageRef& operator=(const ResourceStorageRef& other)
		{
			if (this != &other)
			{
				if (storage)
					storage->RemoveReference();
				storage = other.storage;
				if (storage)
					storage->AddReference();
			}
			return *this;
		}

		FORCE_INLINE bool operator ==(const ResourceStorageRef& other) const
		{
			return storage == other.storage;
		}

		FORCE_INLINE bool operator !=(const ResourceStorageRef& other) const
		{
			return storage != other.storage;
		}

		FORCE_INLINE operator ResourceStorage* () const
		{
			return storage;
		}

		FORCE_INLINE operator bool() const
		{
			return storage != nullptr;
		}

		FORCE_INLINE ResourceStorage* operator->() const
		{
			return storage;
		}

		FORCE_INLINE ResourceStorage* Get() {
			return storage;
		}

		void reset()
		{
			if (storage)
			{
				storage->RemoveReference();
				storage = nullptr;
			}
		}
	};
}