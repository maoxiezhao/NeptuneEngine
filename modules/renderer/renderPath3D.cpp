#include "renderPath3D.h"
#include "renderScene.h"
#include "renderer.h"
#include "imageUtil.h"
#include "shaderInterop_postprocess.h"

#include "passes\preDepthPass.h"
#include "passes\visibilityPass.h"
#include "passes\lightPass.h"
#include "passes\opaquePass.h"
#include "passes\transparentPass.h"
#include "passes\postprocessingPass.h"

namespace VulkanTest
{
	RenderPath3D::RenderPath3D()
	{
		passArray.push_back(PreDepthPass::Instance());
		passArray.push_back(VisibilityPass::Instance());
		passArray.push_back(LightPass::Instance());
		passArray.push_back(OpaquePass::Instance());
		passArray.push_back(TransparentPass::Instance());
		passArray.push_back(PostprocessingPass::Instance());

		for (I32 i = 0; i < passArray.size(); i++)
		{
			if (!passArray[i]->Init())
			{
				Logger::Error("Failed to intialize render pass.");
				passArray.clear();
				return;
			}
		}
	}

	RenderPath3D::~RenderPath3D()
	{
		for (I32 i = 0; i < passArray.size(); i++)
			passArray[i]->Dispose();
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
	}

	void RenderPath3D::SetupPasses(RenderGraph& renderGraph)
	{
		GPU::DeviceVulkan* device = wsi->GetDevice();

		// Render target attachment
		rtAttachment.format = backbufferDim.format;
		rtAttachment.sizeX = (F32)backbufferDim.width;
		rtAttachment.sizeY = (F32)backbufferDim.height;
		
		// Depth attachement
		depthAttachment.format = device->GetDefaultDepthStencilFormat();
		depthAttachment.sizeX = (F32)backbufferDim.width;
		depthAttachment.sizeY = (F32)backbufferDim.height;

		// Setup render passes
		for (auto pass : passArray)
			pass->Setup(renderGraph, *this);

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

	void RenderPath3D::PrepareResources()
	{
		GPU::DeviceVulkan* device = wsi->GetDevice();
		auto& renderGraph = GetRenderGraph();
		camera->bufferLightTileBindless = device->CreateBindlessStroageBuffer(*renderGraph.TryGetPhysicalBuffer("lightTiles"));
		camera->textureDepthBindless = device->CreateBindlessSampledImage(*renderGraph.TryGetPhysicalTexture("depthCopy"), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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