#pragma once

#include "client\app\app.h"
#include "core\platform\platform.h"
#include "gpu\vulkan\device.h"

namespace VulkanTest
{
namespace ImGuiRenderer
{
	void Initialize(App& app);
	void Uninitialize();
	void BeginFrame();
	void EndFrame();
	void Render(GPU::CommandList* cmd);
}
}