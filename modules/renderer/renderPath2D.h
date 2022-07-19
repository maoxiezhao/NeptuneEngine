#pragma once

#include "renderPathGraph.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderPath2D : public RenderPathGraph
	{
	public:
		void Update(float dt) override;
		void FixedUpdate() override;

		const String& GetRenderResult2D()const {
			return lastRenderPassRT2D;
		}

		String SetRenderResult2D(const char* name)
		{
			lastRenderPassRT2D = name;
			return name;
		}

	protected:
		void SetupPasses(RenderGraph& renderGraph) override;
		void SetupComposeDependency(RenderPass& composePass) override;
		void Compose(RenderGraph& renderGraph, GPU::CommandList* cmd) override;
	
	private:
		String lastRenderPassRT2D;
	};
}