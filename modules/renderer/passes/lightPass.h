#pragma once

#include "renderGraphPass.h"
#include "content\resources\shader.h"

namespace VulkanTest
{
	class LightPass : public RenderGraphPass<LightPass>
	{
	public:
		void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) override;

		static const String LightTileRes;
	};
}