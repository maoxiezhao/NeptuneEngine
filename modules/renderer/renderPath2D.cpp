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

	void RenderPath2D::SetupPasses(RenderGraph& renderGraph)
	{
	}

	void RenderPath2D::Compose(RenderGraph& renderGraph, GPU::CommandList* cmd)
	{
	}
}