#pragma once

#include "gpu\vulkan\device.h"
#include "renderer\renderScene.h"

namespace VulkanTest
{
namespace Renderer
{
	void Initialize();
	UniquePtr<RenderScene> CreateScene(World& world);
}
}