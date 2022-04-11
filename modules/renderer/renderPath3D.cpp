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

	void RenderPath3D::Setup(RenderGraph& renderGraph)
	{
		RenderPath2D::Setup(renderGraph);
	}

	void RenderPath3D::Compose(RenderGraph& renderGraph, GPU::CommandList* cmd)
	{
		RenderPath2D::Compose(renderGraph, cmd);
	}
}