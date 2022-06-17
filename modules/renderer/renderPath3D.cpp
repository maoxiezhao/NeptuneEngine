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
		AttachmentInfo rtAttachmentInfo;
		rtAttachmentInfo.format = backbufferDim.format;
		rtAttachmentInfo.sizeX = (F32)backbufferDim.width;
		rtAttachmentInfo.sizeY = (F32)backbufferDim.height;
		
		// Main opaque pass
		auto& opaquePass = renderGraph.AddRenderPass("Opaue", RenderGraphQueueFlag::Graphics);
		opaquePass.WriteColor("rtFinal3D", rtAttachmentInfo, "rtFinal3D");
		opaquePass.SetBuildCallback([&](GPU::CommandList& cmd) {

			//VkViewport viewport = {};
			//viewport.x = 0.0f;
			//viewport.y = 0.0f;
			//viewport.width = rtAttachmentInfo.sizeX;
			//viewport.height = rtAttachmentInfo.sizeY;
			//viewport.minDepth = 0.0f;
			//viewport.maxDepth = 1.0f;
			//cmd.SetViewport(viewport);

			cmd.SetDefaultOpaqueState();
			cmd.SetProgram("screenVS.hlsl", "screenPS.hlsl");
			cmd.Draw(3);
		});

		AddOutputColor("rtFinal3D");
		RenderPath2D::SetupPasses(renderGraph);
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