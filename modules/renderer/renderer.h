#pragma once

#include "enums.h"
#include "gpu\vulkan\device.h"
#include "core\plugin\plugin.h"
#include "core\collections\array.h"
#include "culling.h"
#include "math\math.hpp"

namespace VulkanTest
{
	class RenderPath;
	class RenderScene;

	struct VULKAN_TEST_API RendererPlugin : public IPlugin
	{
		virtual void ActivePath(RenderPath* renderPath) = 0;
		virtual void Render() = 0;
		virtual GPU::DeviceVulkan* GetDevice() = 0;
		virtual RenderScene* GetScene() = 0;
	};

	namespace Renderer
	{
		void Initialize(Engine& engine);
		void Uninitialize();

		void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta);
		void UpdateRenderData(const Visibility& visible, GPU::CommandList& cmd);

		GPU::DeviceVulkan* GetDevice();
		RenderScene* GetScene();

		const GPU::BlendState& GetBlendState(BlendStateTypes types);
		const GPU::RasterizerState& GetRasterizerState(RasterizerStateTypes types);

		enum class RenderQueueType
		{
			Opaque = 1 << 0,
			Transparent = 1 << 1,
			All = Opaque | Transparent
		};

		void DrawScene(GPU::CommandList& cmd);
		void DrawTest(GPU::CommandList& cmd);

		// Plugin interface
		RendererPlugin* CreatePlugin(Engine& engine);
	}
}