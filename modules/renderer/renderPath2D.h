#pragma once

#include "renderPathGraph.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderPath2D : public RenderPathGraph
	{
	public:
		void Update(float dt) override;
		void FixedUpdate() override;

		const char* GetRenderResult2D()const {
			return lastRenderPassRT.c_str();
		}

		const char* SetRenderResult2D(const char* name)
		{
			lastRenderPassRT = name;
			return name;
		}

	protected:
		void SetupPasses(RenderGraph& renderGraph) override;
		void SetupComposeDependency(RenderPass& composePass) override;
		void Compose(RenderGraph& renderGraph, GPU::CommandList* cmd) override;
	
	private:
		String lastRenderPassRT;
	};
}