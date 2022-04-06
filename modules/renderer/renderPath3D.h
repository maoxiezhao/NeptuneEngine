#pragma once

#include "renderPath2D.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderPath3D : public RenderPath2D
	{
	public:
		void Update(float dt) override;
		void FixedUpdate() override;

	protected:
		void SetupRenderGraph(RenderGraph& renderGraph) override;
	};
}