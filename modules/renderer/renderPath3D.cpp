#include "renderPath3D.h"
#include "renderScene.h"
#include "renderer.h"
#include "imageUtil.h"

namespace VulkanTest
{
	void RenderPath3D::Update(float dt)
	{
		RenderScene* scene = GetScene();
		ASSERT(scene != nullptr);

		RenderPath2D::Update(dt);

		// Update main camera
		camera = scene->GetMainCamera();
		ASSERT(camera != nullptr);
		camera->width = (F32)currentBufferSize.x;
		camera->height = (F32)currentBufferSize.y;
		camera->UpdateCamera();

		// Culling for main camera
		visibility.Clear();
		visibility.camera = camera;
		visibility.flags = Visibility::ALLOW_EVERYTHING;
		scene->UpdateVisibility(visibility);

		// Update per frame data
		Renderer::UpdateFrameData(visibility, *scene, dt);
	}

	void RenderPath3D::SetupPasses(RenderGraph& renderGraph)
	{
		GPU::DeviceVulkan* device = wsi->GetDevice();

		AttachmentInfo rtAttachmentInfo;
		rtAttachmentInfo.format = backbufferDim.format;
		rtAttachmentInfo.sizeX = (F32)backbufferDim.width;
		rtAttachmentInfo.sizeY = (F32)backbufferDim.height;
		
		AttachmentInfo depth;
		depth.format = device->GetDefaultDepthFormat();
		depth.sizeX = (F32)backbufferDim.width;
		depth.sizeY = (F32)backbufferDim.height;

		// Main opaque pass
		auto& opaquePass = renderGraph.AddRenderPass("Opaue", RenderGraphQueueFlag::Graphics);
		opaquePass.WriteColor("rtFinal3D", rtAttachmentInfo, "rtFinal3D");
		opaquePass.WriteDepthStencil("depth", depth);
		opaquePass.SetClearDepthStencilCallback([](VkClearDepthStencilValue* value) {
			if (value != nullptr)
			{
				value->depth = 0.0f;
				value->stencil = 1.0f;
			}
			return true;
		});
		opaquePass.SetBuildCallback([&](GPU::CommandList& cmd) {

			GPU::Viewport viewport;
			viewport.width = (F32)backbufferDim.width;
			viewport.height = (F32)backbufferDim.height;
			cmd.SetViewport(viewport);

			Renderer::DrawModel(cmd);
		});

		AddOutputColor("rtFinal3D");
		RenderPath2D::SetupPasses(renderGraph);
	}

	void RenderPath3D::UpdateRenderData()
	{
		GPU::DeviceVulkan* device = wsi->GetDevice();
		auto cmd = device->RequestCommandList(GPU::QueueType::QUEUE_TYPE_GRAPHICS);
		Renderer::UpdateRenderData(*cmd);
		device->Submit(cmd);
	}

	void RenderPath3D::Compose(RenderGraph& renderGraph, GPU::CommandList* cmd)
	{
		auto res = renderGraph.GetOrCreateTexture("rtFinal3D");
		auto& imgFinal3D = renderGraph.GetPhysicalTexture(res);

		ImageUtil::Params params = {};

		cmd->BeginEvent("Compose3D");
		ImageUtil::Draw(imgFinal3D.GetImage(), params, *cmd);
		cmd->EndEvent();

		RenderPath2D::Compose(renderGraph, cmd);
	}
}