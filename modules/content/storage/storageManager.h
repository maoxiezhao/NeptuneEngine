#pragma once

#include "resourceStorage.h"
#include "core\collections\hashMap.h"

namespace VulkanTest
{
	class ResourceManager;

	class VULKAN_TEST_API StorageManager
	{
	public:
		static ResourceStorageRef TryGetStorage(const Path& path, bool isCompiled = true);
		static ResourceStorageRef GetStorage(const Path& path, bool doLoad = false, bool isCompiled = true);
	};
}