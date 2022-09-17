#include "opaquePass.h"
#include "renderer.h"
#include "renderPath3D.h"
#include "lightPass.h"

namespace VulkanTest
{
	const String OpaquePass::RtOpaqueRes = "rtOpaque";

	void OpaquePass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		AttachmentInfo rtAttachment = renderPath.GetAttachmentRT();
		AttachmentInfo depthAttachment = renderPath.GetAttachmentDepth();

		auto& pass = renderGraph.AddRenderPass("Opaue", RenderGraphQueueFlag::Graphics);
		pass.WriteColor(RtOpaqueRes, rtAttachment, RtOpaqueRes);
		pass.ReadDepthStencil(renderPath.GetDepthStencil());
		pass.ReadStorageBufferReadonly(LightPass::LightTileRes);

		// OpaquePass submit before the transparentPass
		pass.AddProxyOutput("opaque", VK_PIPELINE_STAGE_NONE_KHR);

		pass.SetBuildCallback([rtAttachment, &renderPath, &renderGraph](GPU::CommandList& cmd) {

			GPU::Viewport viewport;
			viewport.width = rtAttachment.sizeX;
			viewport.height = rtAttachment.sizeY;
			cmd.SetViewport(viewport);

			Renderer::BindCameraCB(*renderPath.camera, cmd);
			Renderer::DrawScene(renderPath.visibility, RENDERPASS_MAIN, cmd);
			Renderer::DrawSky(*renderPath.GetScene(), cmd);
		});

		renderPath.SetRenderResult3D(RtOpaqueRes);
	}
}
