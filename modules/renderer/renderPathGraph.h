#pragma once

#include "renderer\renderPath.h"
#include "renderer\renderGraph.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderPathGraph : public RenderPath
	{
	public:
		RenderPathGraph() = default;
		virtual ~RenderPathGraph();

		void Update(float dt)override;
		void Render() override;

		virtual void ResizeBuffers();
		void DisableSwapchain();

		RenderGraph& GetRenderGraph() {
			return renderGraph;
		}

	protected:
		virtual void SetupPasses(RenderGraph& renderGraph) = 0;
		virtual void UpdateRenderData();
		virtual void BeforeRender();
		virtual void SetupComposeDependency(RenderPass& composePass) = 0;
		virtual void Compose(RenderGraph& renderGraph, GPU::CommandList* cmd) = 0;

		virtual U32x2 GetInternalResolution()const;

		U32x2 currentBufferSize {};
		ResourceDimensions backbufferDim;

	private:
		RenderGraph renderGraph;
		AttachmentInfo backInfo;
		bool swapchainDisable = false;
	};
}