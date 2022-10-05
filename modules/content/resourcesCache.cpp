#include "resourcesCache.h"
#include "resourceManager.h"
#include "core\serialization\fileReadStream.h"
#include "core\serialization\fileWriteStream.h"
#include "core\utils\deleteHandler.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
	const U32 ResourcesCache::FILE_VERSION = 0x01;

	ResourcesCache::ResourcesCache()
	{
	}

	ResourcesCache::~ResourcesCache()
	{
	}

	void ResourcesCache::Initialize()
	{
#ifdef CJING3D_EDITOR
		path = Globals::ProjectCacheFolder / "resource_cache.bin";
#else
		path = Globals::ProjectContentFolder / "resource_cache.bin";
#endif
		Logger::Info("Load resource cache %s", path.c_str());

		if (!FileSystem::FileExists(path.c_str()))
		{
			isDirty = true;
			Logger::Warning("Failed find resource cache file.");
			return;
		}

		auto stream = FileReadStream::Open(path.c_str());
		DeleteHandler toDelete(stream);
		
		// Version
		I32 version;
		stream->Read(version);
		if (version != FILE_VERSION)
		{
			Logger::Warning("Unsupported resource cache.");
			return;
		}

		// Flags
		ResorucesCacheFlags flags;
		stream->Read(flags);

		ScopedMutex lock(mutex);
		isDirty = false;

		// Read resource registry
		I32 count;
		stream->Read(count);
		resourceRegistry.clear();
		for (int i = 0; i < count; i++)
		{
			Entry entry;
			stream->Read(entry.info.guid);
			stream->Read(entry.info.type);

			I32 strSize;
			stream->Read(strSize);
			char path[MAX_PATH_LENGTH];
			path[strSize] = 0;
			stream->Read(path, strSize);
			entry.info.path = Path(path);

			// Convert to absolute path
			if (flags == ResorucesCacheFlags::RelativePaths && !entry.info.path.IsEmpty())
				entry.info.path = Globals::StartupFolder / entry.info.path;

			resourceRegistry.insert(entry.info.guid, entry);
		}

		// Read paths mapping
		stream->Read(count);
		pathHashMapping.clear();
		for (int i = 0; i < count; i++)
		{
			Guid id;
			stream->Read(id);
			I32 strSize;
			stream->Read(strSize);
			char path[MAX_PATH_LENGTH];
			path[strSize] = 0;
			stream->Read(path, strSize);

			// Convert to absolute path
			Path mappedPath;
			if (flags == ResorucesCacheFlags::RelativePaths && strSize > 0)
				mappedPath = Globals::StartupFolder / path;
			else
				mappedPath = path;

			pathHashMapping.insert(mappedPath, id);
		}

		Logger::Info("Resource cache loaded");
	}

	bool ResourcesCache::Save()
	{
#ifdef CJING3D_EDITOR
		if (!isDirty && FileSystem::FileExists(path.c_str()))
			return false;

		ScopedMutex lock(mutex);
		if (!Save(path, resourceRegistry, pathHashMapping))
			return false;

		isDirty = false;
		return true;
#endif
	}

	bool ResourcesCache::Save(const Path& path, const HashMap<Guid, Entry>& registry, const HashMap<Path, Guid>& pathMapping, ResorucesCacheFlags flags)
	{
		PROFILE_FUNCTION();
		Logger::Info("Saving resouce cache %s", path.c_str());

		auto stream = FileWriteStream::Open(path.c_str());
		if (stream == nullptr)
			return false;

		// Version
		stream->Write((I32)FILE_VERSION);

		// Flags
		stream->Write(flags);

		// Registry
		stream->Write((I32)registry.count());
		for (const auto& e : registry)
		{
			stream->Write(e.info.guid);
			stream->Write(e.info.type);

			const I32 len = (I32)StringLength(e.info.path.c_str());
			stream->Write(len);
			stream->Write(e.info.path.c_str(), len);
		}

		// PathMapping
		stream->Write((I32)pathMapping.count());
		for (auto it = pathMapping.begin(); it != pathMapping.end(); ++it)
		{
			stream->Write(it.value());
			
			const Path& path = it.key();
			const I32 len = (I32)StringLength(path.c_str());
			stream->Write(len);
			stream->Write(path.c_str(), len);
		}

		stream->Flush();
		CJING_DELETE(stream);
		return true;
	}

	void ResourcesCache::Register(ResourceStorage* storage)
	{
		ASSERT(storage);
		PROFILE_FUNCTION();

		auto entry = storage->GetResourceEntry();
		ASSERT(entry.guid.IsValid());

		ScopedMutex lock(mutex);
		auto storagePath = storage->GetPath();
		for (auto it = resourceRegistry.begin(); it != resourceRegistry.end(); ++it)
		{
			if (it.value().info.path == storagePath)
				resourceRegistry.erase(it);
		}

		// Find resource guid collison
		bool hasCollision = false;
		ResourceInfo resInfo;
		if (Find(entry.guid, resInfo))
		{
			Logger::Warning("Founded duplicated resource %d %d %s", entry.guid.GetHash(), entry.type.GetHashValue(), storagePath.c_str());
			hasCollision = true;
			ASSERT(false);

			// TODO Change guid of resource to avoid collisio
		}

		// Register resource entry
		Logger::Info("Register resource %d %d %s", entry.guid.GetHash(), entry.type.GetHashValue(), storagePath.c_str());
		Entry e = {};
		e.info.guid = entry.guid;
		e.info.type = entry.type;
		e.info.path = storagePath;
		resourceRegistry.insert(entry.guid, e);
		pathHashMapping.insert(path, entry.guid);

		isDirty = true;
	}

	void ResourcesCache::Register(const Guid& guid, const ResourceType& type, const Path& path)
	{
		PROFILE_FUNCTION();
		ScopedMutex lock(mutex);

		bool newRegistry = true;
		for (auto it = resourceRegistry.begin(); it != resourceRegistry.end(); ++it)
		{
			auto& entry = it.value();
			if (entry.info.guid == guid)
			{
				if (entry.info.path != path)
				{
					entry.info.path = path;
					isDirty = true;
				}
				if (entry.info.type != type)
				{
					entry.info.type = type;
					isDirty = true;
				}
				newRegistry = false;
				break;
			}

			if (entry.info.path == path)
			{
				if (entry.info.guid != guid)
				{
					entry.info.path = path;
					isDirty = true;
				}
				if (entry.info.type != type)
				{
					entry.info.type = type;
					isDirty = true;
				}
				newRegistry = false;
				break;
			}
		}

		if (newRegistry)
		{
			Logger::Info("Register resource %d %d %s", guid.GetHash(), type.GetHashValue(), path.c_str());
			Entry e = {};
			e.info.guid = guid;
			e.info.type = type;
			e.info.path = path;
			resourceRegistry.insert(guid, e);
			pathHashMapping.insert(path, guid);
			isDirty = true;
		}
	}

	bool ResourcesCache::Delete(const Path& path, ResourceInfo* resInfo)
	{
		bool ret = false;
		ScopedMutex lock(mutex);

		for (auto it = resourceRegistry.begin(); it != resourceRegistry.end(); ++it)
		{
			if (it.value().info.path == path)
			{
				if (resInfo != nullptr)
					*resInfo = it.value().info;

				resourceRegistry.erase(it);
				isDirty = true;
				ret = true;
				const auto& info = it.value().info;
				Logger::Info("Delete resource %d %d %s", info.guid.GetHash(), info.type.GetHashValue(), path.c_str());
				break;
			}
		}
		return ret;
	}

	bool ResourcesCache::Delete(const Guid& guid, ResourceInfo* resInfo)
	{
		bool ret = false;
		ScopedMutex lock(mutex);
		auto it = resourceRegistry.find(guid);
		if (it.isValid())
		{
			if (resInfo != nullptr)
				*resInfo = it.value().info;

			resourceRegistry.erase(it);
			isDirty = true;
			ret = true;
			const auto& info = it.value().info;
			Logger::Info("Delete resource %d %d %s", info.guid.GetHash(), info.type.GetHashValue(), path.c_str());

		}
		return ret;
	}

	bool ResourcesCache::Find(const Path& path, ResourceInfo& resInfo)
	{
		PROFILE_FUNCTION();
		mutex.Lock();
		Guid guid;
		if (pathHashMapping.tryGet(path, guid))
		{
			mutex.Unlock();
			return Find(guid, resInfo);
		}
		mutex.Unlock();

		ScopedMutex lock(mutex);
		for (auto it = resourceRegistry.begin(); it != resourceRegistry.end(); ++it)
		{
			auto& entry = it.value();
			if (entry.info.path == path)
			{
				resInfo = entry.info;
				return true;
			}
		}

		return false;
	}

	bool ResourcesCache::Find(const Guid& id, ResourceInfo& resInfo)
	{
		PROFILE_FUNCTION();
		ScopedMutex lock(mutex);
		auto it = resourceRegistry.find(id);
		if (it.isValid())
		{
			resInfo = it.value().info;
			return true;
		}
		return false;
	}
}