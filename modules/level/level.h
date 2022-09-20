#pragma once

#include "core\common.h"
#include "scene.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Level
	{
	public:
		static bool SaveScene(Scene* scene, rapidjson_flax::StringBuffer& outData);
	};
}