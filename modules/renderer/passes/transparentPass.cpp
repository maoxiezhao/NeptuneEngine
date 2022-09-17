#include "transparentPass.h"
#include "renderer.h"
#include "debugDraw.h"
#include "renderPath3D.h"
#include "opaquePass.h"

namespace VulkanTest
{
	void TransparentPass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		AttachmentInfo rtAttachment = renderPath.GetAttachmentRT();
		AttachmentInfo depthAttachment = renderPath.GetAttachmentDepth();

		auto& transparentPass = renderGraph.AddRenderPass("Transparent", RenderGraphQueueFlag::Graphics);
		transparentPass.WriteColor(OpaquePass::RtOpaqueRes, rtAttachment);
		transparentPass.ReadDepthStencil(renderPath.GetDepthStencil());
		transparentPass.WriteDepthStencil("finalDepth", depthAttachment);

		// TransparentPass submit after the OpaquePass
		transparentPass.AddProxyInput("opaque", VK_PIPELINE_STAGE_NONE_KHR);

		transparentPass.SetBuildCallback([&](GPU::CommandList& cmd) {

			// Draw debug
			DebugDraw::DrawDebug(*renderPath.GetScene(), *renderPath.camera, cmd);
		});

		renderPath.SetDepthStencil("finalDepth");
	}
}