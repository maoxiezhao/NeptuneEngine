#pragma once

#include "renderGraphPass.h"
#include "content\resources\shader.h"

namespace VulkanTest
{
	class ShadowPass : public RenderGraphPass<ShadowPass>
	{
	private:
		ResPtr<Shader> shader;

		bool Init() override;
		void Dispose() override;

	public:
		void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) override;
	};
}