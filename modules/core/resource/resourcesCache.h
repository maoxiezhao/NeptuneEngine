#pragma once

#include "core\common.h"
#include "core\collections\hashMap.h"
#include "core\utils\guid.h"
#include "resource.h"
#include "resourceInfo.h"
#include "storageManager.h"

namespace VulkanTest
{
	class ResourceManager;

	class VULKAN_TEST_API ResourcesCache
	{
	public:
		static const U32 FILE_VERSION;

		struct Entry
		{
			ResourceInfo info;
		};

		ResourcesCache(ResourceManager& resManager_);
		~ResourcesCache();

		void Initialize();
		bool Save();
		bool Find(const Path& path, ResourceInfo& resInfo);
		bool Find(const Guid& id, ResourceInfo& resInfo);

		void Register(ResourceStorage* storage);
		void Register(const Guid& guid, const ResourceType& type, const Path& path);

		bool Delete(const Path& path, ResourceInfo* resInfo);
		bool Delete(const Guid& path, ResourceInfo* resInfo);

		static bool Save(FileSystem* fs, const Path& path, const HashMap<Guid, Entry>& registry, const HashMap<U64, Guid>& pathMapping);

	private:
		ResourceManager& resManager;
		HashMap<Guid, Entry> resourceRegistry;
		HashMap<U64, Guid> pathHashMapping;
		Mutex mutex;
		Path path;
		bool isDirty = false;
	};
}