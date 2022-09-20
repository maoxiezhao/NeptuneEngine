#pragma once

#include "core\common.h"
#include "scene.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Level
	{
	public:
		static Array<Scene*>& GetScenes();
		static Scene* FindScene(const Guid& guid);
		static bool LoadScene(const Guid& guid);
		static bool LoadScene(const Path& path);
		static bool SaveScene(Scene* scene, rapidjson_flax::StringBuffer& outData);
	};
}