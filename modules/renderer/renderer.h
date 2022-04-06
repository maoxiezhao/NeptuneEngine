#pragma once

#include "gpu\vulkan\device.h"
#include "core\plugin\plugin.h"

namespace VulkanTest
{
	class RenderPath;

	struct VULKAN_TEST_API RendererPlugin : public IPlugin
	{
		virtual void ActivePath(RenderPath* renderPath) = 0;
		virtual void Render() = 0;
	};

	namespace Renderer
	{
		void Initialize();
		void Uninitialize();

		RendererPlugin* CreatePlugin(Engine& engine);
	}
}