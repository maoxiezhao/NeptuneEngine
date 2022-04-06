#include "renderPath2D.h"

namespace VulkanTest
{
	void RenderPath2D::Update(float dt)
	{
		RenderPathGraph::Update(dt);
	}

	void RenderPath2D::FixedUpdate()
	{
		RenderPathGraph::FixedUpdate();
	}

	void RenderPath2D::SetupRenderGraph(RenderGraph& renderGraph)
	{
	}
}