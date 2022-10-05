#pragma once

#include "core\common.h"
#include "core\collections\hashMap.h"
#include "core\types\guid.h"
#include "resource.h"
#include "resourceInfo.h"
#include "storage\storageManager.h"

namespace VulkanTest
{
	class ResourceManager;

	enum class ResorucesCacheFlags
	{
		None = 0,
		RelativePaths = 1,
	};

	class VULKAN_TEST_API ResourcesCache
	{
	public:
		static const U32 FILE_VERSION;

		struct Entry
		{
			ResourceInfo info;
		};

		ResourcesCache();
		~ResourcesCache();

		void Initialize();
		bool Save();
		bool Find(const Path& path, ResourceInfo& resInfo);
		bool Find(const Guid& id, ResourceInfo& resInfo);

		void Register(ResourceStorage* storage);
		void Register(const Guid& guid, const ResourceType& type, const Path& path);

		bool Delete(const Path& path, ResourceInfo* resInfo);
		bool Delete(const Guid& path, ResourceInfo* resInfo);

		static bool Save(const Path& path, const HashMap<Guid, Entry>& registry, const HashMap<Path, Guid>& pathMapping, ResorucesCacheFlags flags = ResorucesCacheFlags::None);

	private:
		HashMap<Guid, Entry> resourceRegistry;
		HashMap<Path, Guid> pathHashMapping;
		Mutex mutex;
		Path path;
		bool isDirty = false;
	};
}