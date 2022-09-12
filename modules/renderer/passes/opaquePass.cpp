#include "opaquePass.h"
#include "renderer.h"
#include "renderPath3D.h"

namespace VulkanTest
{
	void OpaquePass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		AttachmentInfo rtAttachment = renderPath.GetAttachmentRT();
		AttachmentInfo depthAttachment = renderPath.GetAttachmentDepth();

		auto& pass = renderGraph.AddRenderPass("Opaue", RenderGraphQueueFlag::Graphics);
		pass.WriteColor("rtOpaque", rtAttachment, "rtOpaque");
		pass.SetClearColorCallback(DefaultClearColorFunc);
		pass.AddProxyOutput("opaque", VK_PIPELINE_STAGE_NONE_KHR);
		pass.ReadDepthStencil(renderPath.GetDepthStencil());
		pass.ReadStorageBufferReadonly("lightTiles");

		pass.SetBuildCallback([rtAttachment, &renderPath, &renderGraph](GPU::CommandList& cmd) {

			GPU::Viewport viewport;
			viewport.width = rtAttachment.sizeX;
			viewport.height = rtAttachment.sizeY;
			cmd.SetViewport(viewport);

			Renderer::BindCameraCB(*renderPath.camera, cmd);
			Renderer::DrawScene(cmd, renderPath.visibility, RENDERPASS_MAIN);
		});

		renderPath.SetRenderResult3D("rtOpaque");
	}
}
