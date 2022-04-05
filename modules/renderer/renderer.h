#pragma once

#include "gpu\vulkan\device.h"
#include "core\plugin\plugin.h"

namespace VulkanTest
{
	class RenderPath;

	namespace Renderer
	{
		void Initialize();
		void Uninitialize();
		void ActiveRenderPath(Engine& engine, RenderPath* renderPath);

		IPlugin* CreatePlugin(Engine& engine);
	}
}