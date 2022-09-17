#pragma once

#include "renderGraphPass.h"
#include "content\resources\shader.h"

namespace VulkanTest
{
	class TransparentPass : public RenderGraphPass<TransparentPass>
	{
	public:
		void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) override;
	};
}