#pragma once

#include "renderer\renderPath.h"
#include "renderer\renderGraph.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderPathGraph : public RenderPath
	{
	public:
		RenderPathGraph() = default;
		virtual ~RenderPathGraph() = default;

		void Start()override;
		void Stop()override;
		void Update(float dt)override;
		void Render() override;

	protected:
		virtual void ResizeBuffers();
		virtual void SetupRenderGraph(RenderGraph& renderGraph) = 0;

	private:
		RenderGraph renderGraph;
		U32x2 currentBufferSize {};
	};
}