#pragma once

#include "resourceStorage.h"
#include "core\collections\hashMap.h"

namespace VulkanTest
{
	class ResourceManager;

	class VULKAN_TEST_API StorageManager
	{
	public:
		static ResourceStorageRef GetStorage(const Path& path, ResourceManager& resManager);
	};
}