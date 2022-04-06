#pragma once

#include "renderPathGraph.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderPath2D : public RenderPathGraph
	{
	public:
		void Update(float dt) override;
		void FixedUpdate() override;

	protected:
		void SetupRenderGraph(RenderGraph& renderGraph) override;
	};
}