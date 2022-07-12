#include "renderPathGraph.h"
#include "core\jobsystem\jobsystem.h"
#include "core\events\event.h"

namespace VulkanTest
{
	RenderPathGraph::~RenderPathGraph()
	{
		renderGraph.Reset();
	}

	void RenderPathGraph::Update(float dt)
	{
		U32x2 internalResolution = GetInternalResolution();
		if (currentBufferSize.x != internalResolution.x || currentBufferSize.y != internalResolution.y)
			ResizeBuffers();

		RenderPath::Update(dt);
	}

	void RenderPathGraph::Render()
	{
		UpdateRenderData();

		GPU::DeviceVulkan* device = wsi->GetDevice();
		renderGraph.SetupAttachments(*device, &wsi->GetImageView());
		Jobsystem::JobHandle handle;
		renderGraph.Render(*device, handle);
		Jobsystem::Wait(&handle);

		device->MoveReadWriteCachesToReadOnly();
	}

	void RenderPathGraph::DisableSwapchain()
	{
		swapchainDisable = true;
	}

	void RenderPathGraph::ResizeBuffers()
	{
		currentBufferSize = GetInternalResolution();

		GPU::DeviceVulkan* device = wsi->GetDevice();
		renderGraph.Reset();
		renderGraph.SetDevice(device);

		if (currentBufferSize.x == 0 || currentBufferSize.y == 0)
			return;

		backbufferDim.width = currentBufferSize.x;
		backbufferDim.height = currentBufferSize.y;
		backbufferDim.format = wsi->GetSwapchain().swapchainFormat;
		renderGraph.SetBackbufferDimension(backbufferDim);

		AttachmentInfo backInfo;
		backInfo.format = backbufferDim.format;
		backInfo.sizeX = (F32)backbufferDim.width;
		backInfo.sizeY = (F32)backbufferDim.height;

		// Setup render graph
		outputColors.clear();
		SetupPasses(renderGraph);
		
		// Compose
		auto& composePass = renderGraph.AddRenderPass("Compose", RenderGraphQueueFlag::Graphics);
		composePass.WriteColor("back", backInfo, "back");
		for(const auto& color : outputColors)
			composePass.ReadTexture(color.c_str());

		composePass.SetClearColorCallback([](U32 index, VkClearColorValue* value) 
		{
			if (value != nullptr)
			{
				value->float32[0] = 0.0f;
				value->float32[1] = 0.0f;
				value->float32[2] = 0.0f;
				value->float32[3] = 1.0f;
			}
			return true;
		});

		composePass.SetBuildCallback([&](GPU::CommandList& cmd) 
		{
			GPU::Viewport viewport;
			viewport.width = (F32)backbufferDim.width;
			viewport.height = (F32)backbufferDim.height;
			cmd.SetViewport(viewport);

			Compose(renderGraph, &cmd);
		});

		if (swapchainDisable)
			renderGraph.DisableSwapchain();

		renderGraph.SetBackBufferSource("back");
		renderGraph.Bake();
	}

	void RenderPathGraph::UpdateRenderData()
	{
	}

	U32x2 RenderPathGraph::GetInternalResolution() const
	{
		auto platform = wsi->GetPlatform();
		return U32x2(platform->GetWidth(), platform->GetHeight());
	}
}