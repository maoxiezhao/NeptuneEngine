#pragma once

#include "core\platform\platform.h"
#include "gpu\vulkan\device.h"
#include "renderer\renderPath3D.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
namespace ImGuiRenderer
{
	void Initialize();
	void Uninitialize();
	void Render(GPU::CommandList* cmd);
}
}