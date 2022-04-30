#include "renderPath3D.h"
#include "renderScene.h"
#include "renderer.h"
#include "imageUtil.h"

namespace VulkanTest
{
	void RenderPath3D::Update(float dt)
	{
		ASSERT(scene != nullptr);

		RenderPath2D::Update(dt);

		// Update render scene
		if (GetSceneUpdateEnabled()) {
			scene->Update(dt, false);
		}

		camera = scene->GetMainCamera();
		ASSERT(camera != nullptr);
		camera->width = (F32)currentBufferSize.x;
		camera->height = (F32)currentBufferSize.y;
		camera->UpdateCamera();

		// Culling for main camera
		visibility.camera = camera;
		visibility.flags = Visibility::ALLOW_EVERYTHING;
		scene->UpdateVisibility(visibility);

		// Update per frame data
		Renderer::UpdateFrameData(visibility, *scene, dt);
	}

	void RenderPath3D::FixedUpdate()
	{
	}

	void RenderPath3D::SetupPasses(RenderGraph& renderGraph)
	{
		// Prepare frame pass
		auto& prepareFramePass = renderGraph.AddRenderPass("PrepareFrame", RenderGraphQueueFlag::Graphics);
		prepareFramePass.SetBuildCallback([&](GPU::CommandList& cmd) {
			Renderer::UpdateRenderData(visibility, cmd);
		});

		// Depth prepass
		auto& depthPrepassPass = renderGraph.AddRenderPass("DepthPrepass", RenderGraphQueueFlag::Graphics);
		depthPrepassPass.SetBuildCallback([&](GPU::CommandList& cmd) {
			
		});

		// Shadow maps
		auto& shadowMapPass = renderGraph.AddRenderPass("ShadowMap", RenderGraphQueueFlag::Graphics);
		shadowMapPass.SetBuildCallback([&](GPU::CommandList& cmd) {
			
		});
		
		// Main opaque pass
		auto& opaquePass = renderGraph.AddRenderPass("Opaue", RenderGraphQueueFlag::Graphics);
		opaquePass.SetBuildCallback([&](GPU::CommandList& cmd) {
			
		});

		// Transparent
		auto& transparentPass = renderGraph.AddRenderPass("Transparent", RenderGraphQueueFlag::Graphics);
		transparentPass.SetBuildCallback([&](GPU::CommandList& cmd) {
			
		});
		AddOutputColor("final3D");

		RenderPath2D::SetupPasses(renderGraph);
	}

	void RenderPath3D::Compose(RenderGraph& renderGraph, GPU::CommandList* cmd)
	{
		auto res = renderGraph.GetOrCreateTexture("final3D");
		auto& imgFinal3D = renderGraph.GetPhysicalTexture(res);

		ImageUtil::Params params = {};

		cmd->BeginEvent("Compose3D");
		ImageUtil::Draw(imgFinal3D.GetImage(), params, *cmd);
		cmd->EndEvent();

		RenderPath2D::Compose(renderGraph, cmd);
	}
}