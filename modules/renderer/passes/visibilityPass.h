#pragma once

#include "renderGraphPass.h"
#include "content\resources\shader.h"

namespace VulkanTest
{
	class VisibilityPass : public RenderGraphPass<VisibilityPass>
	{
	private:
		ResPtr<Shader> shader;

		bool Init() override;
		void Dispose() override;

	public:
		void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) override;

		static const String DepthCopy;
		static const String DepthLinear;
	};
}