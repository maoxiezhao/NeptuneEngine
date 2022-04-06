#pragma once

#include "core\common.h"
#include "core\scene\world.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderScene : public IScene
	{
	public:
		static UniquePtr<RenderScene> CreateScene(Engine& engine, World& world);
	};
}