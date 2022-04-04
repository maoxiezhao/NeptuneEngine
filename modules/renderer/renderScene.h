#pragma once

#include "core\common.h"
#include "core\scene\world.h"

namespace VulkanTest
{
	class RenderScene : public IScene
	{
	public:
		static UniquePtr<RenderScene> CreateScene(Engine& engine, World& world);
	};
}