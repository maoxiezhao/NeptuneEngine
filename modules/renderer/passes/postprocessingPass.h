#pragma once

#include "renderGraphPass.h"
#include "content\resources\shader.h"

namespace VulkanTest
{
	class PostprocessingPass : public RenderGraphPass<PostprocessingPass>
	{
	public:
		void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) override;
	};
}