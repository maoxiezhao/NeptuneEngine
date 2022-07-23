#include "renderPath2D.h"
#include "renderer.h"
#include "imageUtil.h"

namespace VulkanTest
{
	static bool DefaultClearColorFunc(U32 index, VkClearColorValue* value)
	{
		if (value != nullptr)
		{
			value->float32[0] = 0.0f;
			value->float32[1] = 0.0f;
			value->float32[2] = 0.0f;
			value->float32[3] = 0.0f;
		}
		return true;
	}

	void RenderPath2D::Update(float dt)
	{
		RenderPathGraph::Update(dt);
	}

	void RenderPath2D::FixedUpdate()
	{
		RenderPathGraph::FixedUpdate();
	}

	void RenderPath2D::SetupPasses(RenderGraph& renderGraph)
	{
		GPU::DeviceVulkan* device = wsi->GetDevice();

		AttachmentInfo rtAttachmentInfo;
		rtAttachmentInfo.format = backbufferDim.format;
		rtAttachmentInfo.sizeX = (F32)backbufferDim.width;
		rtAttachmentInfo.sizeY = (F32)backbufferDim.height;

		// Main opaque pass
		auto& renderPass2D = renderGraph.AddRenderPass("RenderPass2D", RenderGraphQueueFlag::Graphics);
		renderPass2D.WriteColor(SetRenderResult2D("rtFinal2D"), rtAttachmentInfo);
		renderPass2D.SetClearColorCallback(DefaultClearColorFunc);
		renderPass2D.SetBuildCallback([&](GPU::CommandList& cmd) {

			GPU::Viewport viewport;
			viewport.width = (F32)backbufferDim.width;
			viewport.height = (F32)backbufferDim.height;
			cmd.SetViewport(viewport);

		});
	}

	void RenderPath2D::SetupComposeDependency(RenderPass& composePass)
	{
		composePass.ReadTexture(GetRenderResult2D());
	}

	void RenderPath2D::Compose(RenderGraph& renderGraph, GPU::CommandList* cmd)
	{
		auto res = renderGraph.GetOrCreateTexture(GetRenderResult2D());
		auto& img = renderGraph.GetPhysicalTexture(res);

		ImageUtil::Params params = {};
		params.EnableFullScreen();
		params.blendFlag = BLENDMODE_PREMULTIPLIED;

		cmd->BeginEvent("Compose2D");
		ImageUtil::Draw(img.GetImage(), params, *cmd);
		cmd->EndEvent();
	}
}