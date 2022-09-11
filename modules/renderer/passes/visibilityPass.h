#pragma once

#include "renderGraphPass.h"
#include "content\resources\shader.h"

namespace VulkanTest
{
	class VisibilityPass : public RenderGraphPass<VisibilityPass>
	{
	public:
		void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) override;
	};
}