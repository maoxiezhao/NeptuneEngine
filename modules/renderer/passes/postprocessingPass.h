#pragma once

#include "renderGraphPass.h"
#include "content\resources\shader.h"

namespace VulkanTest
{
	class PostprocessingPass : public RenderGraphPass<PostprocessingPass>
	{
	private:
		ResPtr<Shader> shaderTonemap;
		ResPtr<Shader> shaderFxaa;

	public:
		void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) override;
		
		bool Init() override;
		void Dispose() override;

		static String RtPostprocess;
	};
}