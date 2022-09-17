#include "preDepthPass.h"
#include "renderer.h"
#include "renderPath3D.h"

namespace VulkanTest
{
	void PreDepthPass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		AttachmentInfo rtAttachment = renderPath.GetAttachmentRT();
		AttachmentInfo depthAttachment = renderPath.GetAttachmentDepth();

		AttachmentInfo rtAttachmentPrimitive = rtAttachment;
		rtAttachmentPrimitive.format = VK_FORMAT_R32_UINT;

		auto& pass = renderGraph.AddRenderPass("PreDepth", RenderGraphQueueFlag::Graphics);
		pass.WriteColor("rtPrimitiveID", rtAttachmentPrimitive, "rtPrimitiveID");
		pass.WriteDepthStencil("depth", depthAttachment);
		pass.SetClearDepthStencilCallback(DefaultClearDepthFunc);
		pass.SetClearColorCallback(DefaultClearColorFunc);

		pass.SetBuildCallback([&renderPath, rtAttachment](GPU::CommandList& cmd) {

			GPU::Viewport viewport;
			viewport.width = rtAttachment.sizeX;
			viewport.height = rtAttachment.sizeY;
			cmd.SetViewport(viewport);

			Renderer::BindCameraCB(*renderPath.camera, cmd);
			Renderer::DrawScene(renderPath.visibility, RENDERPASS_PREPASS, cmd);
		});

		renderPath.SetDepthStencil("depth");
	}
}
