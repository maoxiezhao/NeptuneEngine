#pragma once

#include "renderGraphPass.h"
#include "content\resources\shader.h"

namespace VulkanTest
{
	class PreDepthPass : public RenderGraphPass<PreDepthPass>
	{
	public:
		void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) override;
	};
}