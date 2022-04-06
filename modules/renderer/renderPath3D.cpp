#include "renderPath3D.h"

namespace VulkanTest
{
	void RenderPath3D::Update(float dt)
	{
		RenderPath2D::Update(dt);
	}

	void RenderPath3D::FixedUpdate()
	{
		RenderPath2D::FixedUpdate();
	}

	void RenderPath3D::SetupRenderGraph(RenderGraph& renderGraph)
	{
		RenderPath2D::SetupRenderGraph(renderGraph);
	}
}