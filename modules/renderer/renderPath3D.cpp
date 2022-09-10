#include "renderPath3D.h"
#include "renderScene.h"
#include "renderer.h"
#include "imageUtil.h"
#include "shaderInterop_postprocess.h"

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

	static bool DefaultClearDepthFunc(VkClearDepthStencilValue* value) 
	{
		if (value != nullptr)
		{
			value->depth = 0.0f;
			value->stencil = 0;
		}
		return true;
	}

	void RenderPath3D::Update(float dt)
	{
		GPU::DeviceVulkan* device = wsi->GetDevice();
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

		// Update camera buffers mapping
		auto& renderGraph = GetRenderGraph();
		auto lightTileBuffer = renderGraph.TryGetPhysicalBuffer("LightTiles");
		camera->bufferLightTileBindless = lightTileBuffer ? device->CreateBindlessStroageBuffer(*lightTileBuffer) : GPU::BindlessDescriptorPtr();
	}

	void RenderPath3D::SetupPasses(RenderGraph& renderGraph)
	{
		GPU::DeviceVulkan* device = wsi->GetDevice();

		// Render target attachment
		AttachmentInfo rtAttachment;
		rtAttachment.format = backbufferDim.format;
		rtAttachment.sizeX = (F32)backbufferDim.width;
		rtAttachment.sizeY = (F32)backbufferDim.height;
		
		// Depth attachement
		AttachmentInfo depthAttachment;
		depthAttachment.format = device->GetDefaultDepthStencilFormat();
		depthAttachment.sizeX = (F32)backbufferDim.width;
		depthAttachment.sizeY = (F32)backbufferDim.height;

		// Depth prepass
		{
			AttachmentInfo rtAttachmentPrimitive = rtAttachment;
			rtAttachmentPrimitive.format = VK_FORMAT_R32_UINT;

			auto& pass = renderGraph.AddRenderPass("PreDepth", RenderGraphQueueFlag::Graphics);
			pass.WriteColor("rtPrimitiveID", rtAttachmentPrimitive, "rtPrimitiveID");
			pass.WriteDepthStencil("depth", depthAttachment);
			pass.SetClearDepthStencilCallback(DefaultClearDepthFunc);
			pass.SetClearColorCallback(DefaultClearColorFunc);

			pass.SetBuildCallback([&](GPU::CommandList& cmd) {

				GPU::Viewport viewport;
				viewport.width = (F32)backbufferDim.width;
				viewport.height = (F32)backbufferDim.height;

				cmd.SetViewport(viewport);
				Renderer::BindCameraCB(*camera, cmd);
				Renderer::DrawScene(cmd, visibility, RENDERPASS_PREPASS);
			});

			SetDepthStencil("depth");
		}

		// Lighting pass
		// Renderer::SetupTiledLightCulling(renderGraph, *camera, rtAttachment);

		// Main opaque pass
		{
			auto& pass = renderGraph.AddRenderPass("Opaue", RenderGraphQueueFlag::Graphics);
			pass.WriteColor("rtOpaque", rtAttachment, "rtOpaque");
			pass.SetClearColorCallback(DefaultClearColorFunc);
			pass.AddProxyOutput("opaque", VK_PIPELINE_STAGE_NONE_KHR);
			pass.ReadDepthStencil(GetDepthStencil());
			// pass.ReadStorageBufferReadonly("LightTiles");

			pass.SetBuildCallback([&](GPU::CommandList& cmd) {

				GPU::Viewport viewport;
				viewport.width = (F32)backbufferDim.width;
				viewport.height = (F32)backbufferDim.height;
				cmd.SetViewport(viewport);

				Renderer::BindCameraCB(*camera, cmd);
				Renderer::DrawScene(cmd, visibility, RENDERPASS_MAIN);
			});

			SetRenderResult3D("rtOpaque");
		}

		// Setup render path2D
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
		params.EnableFullScreen();
		params.blendFlag = BLENDMODE_OPAQUE;

		cmd->BeginEvent("Compose3D");
		ImageUtil::Draw(img.GetImage(), params, *cmd);
		cmd->EndEvent();

		RenderPath2D::Compose(renderGraph, cmd);
	}
}