#pragma once

#include "renderGraphPass.h"
#include "content\resources\shader.h"

namespace VulkanTest
{
	class OpaquePass : public RenderGraphPass<OpaquePass>
	{
	public:
		void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) override;
	};
}