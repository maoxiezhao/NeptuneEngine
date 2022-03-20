#pragma once

#include "core\common.h"
#include "ecs\world.h"

namespace VulkanTest
{
	class RenderScene : public IScene
	{
	public:
		static UniquePtr<RenderScene> CreateScene(Engine& engine, World& world);
	};
}