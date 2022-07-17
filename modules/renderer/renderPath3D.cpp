#include "renderPath3D.h"
#include "renderScene.h"
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
			value->float32[3] = 1.0f;
		}
		return true;
	}

	static bool DefaultClearDepthFunc(VkClearDepthStencilValue* value) 
	{
		if (value != nullptr)
		{
			value->depth = 0.0f;
			value->stencil = 1.0f;
		}
		return true;
	}

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
		visibility.scene = scene;
		visibility.camera = camera;
		visibility.flags = Visibility::ALLOW_EVERYTHING;
		scene->UpdateVisibility(visibility);

		// Update per frame data
		Renderer::UpdateFrameData(visibility, *scene, dt, frameCB);
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
		opaquePass.WriteColor(SetRenderResult3D("rtOpaque"), rtAttachmentInfo);
		opaquePass.SetClearColorCallback(DefaultClearColorFunc);
		opaquePass.WriteDepthStencil("depth", depth);
		opaquePass.SetClearDepthStencilCallback(DefaultClearDepthFunc);
		opaquePass.SetBuildCallback([&](GPU::CommandList& cmd) {

			GPU::Viewport viewport;
			viewport.width = (F32)backbufferDim.width;
			viewport.height = (F32)backbufferDim.height;
			cmd.SetViewport(viewport);

			Renderer::BindCameraCB(*camera, cmd);
			Renderer::DrawScene(cmd, visibility);
		});

		// Transparents
		auto& transparentPass = renderGraph.AddRenderPass("Transparent", RenderGraphQueueFlag::Graphics);
		transparentPass.ReadTexture(GetRenderResult3D());
		transparentPass.WriteColor(SetRenderResult3D("rtTransparent"), rtAttachmentInfo);
		transparentPass.ReadDepthStencil("depth");
		transparentPass.WriteDepthStencil("depth", depth);
		transparentPass.SetBuildCallback([&](GPU::CommandList& cmd) {

			GPU::Viewport viewport;
			viewport.width = (F32)backbufferDim.width;
			viewport.height = (F32)backbufferDim.height;
			cmd.SetViewport(viewport);

			Renderer::BindCameraCB(*camera, cmd);
			Renderer::BindCommonResources(cmd);

			cmd.SetDefaultTransparentState();
			cmd.SetProgram("screenVS.hlsl", "screenPS.hlsl");
			cmd.Draw(3);
		});

		RenderPath2D::SetupPasses(renderGraph);
	}

	void RenderPath3D::UpdateRenderData()
	{
		GPU::DeviceVulkan* device = wsi->GetDevice();
		auto cmd = device->RequestCommandList(GPU::QueueType::QUEUE_TYPE_GRAPHICS);
		Renderer::UpdateRenderData(visibility, frameCB, *cmd);
		device->Submit(cmd,  nullptr);
	}

	void RenderPath3D::SetupComposeDependency(RenderPass& composePass)
	{
		composePass.ReadTexture(GetRenderResult3D());
		RenderPath2D::SetupComposeDependency(composePass);
	}

	void RenderPath3D::Compose(RenderGraph& renderGraph, GPU::CommandList* cmd)
	{
		auto res = renderGraph.GetOrCreateTexture(GetRenderResult3D());
		auto& img = renderGraph.GetPhysicalTexture(res);

		ImageUtil::Params params = {};

		cmd->BeginEvent("Compose3D");
		ImageUtil::Draw(img.GetImage(), params, *cmd);
		cmd->EndEvent();

		RenderPath2D::Compose(renderGraph, cmd);
	}
}