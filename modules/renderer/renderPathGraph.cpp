#include "renderPathGraph.h"
#include "core\jobsystem\jobsystem.h"
#include "core\events\event.h"

namespace VulkanTest
{
	void RenderPathGraph::Start()
	{
	}

	void RenderPathGraph::Stop()
	{
		renderGraph.Reset();
	}

	void RenderPathGraph::Update(float dt)
	{
		WSIPlatform* platform = wsi->GetPlatform();
		U32 width = platform->GetWidth();
		U32 height = platform->GetHeight();
		if (currentBufferSize.x != width || currentBufferSize.y != height)
			ResizeBuffers();

		RenderPath::Update(dt);
	}

	void RenderPathGraph::Render()
	{
		GPU::DeviceVulkan* device = wsi->GetDevice();
		renderGraph.SetupAttachments(*device, &device->GetSwapchainView());
		Jobsystem::JobHandle handle = Jobsystem::INVALID_HANDLE;
		renderGraph.Render(*device, handle);
		Jobsystem::Wait(handle);

		device->MoveReadWriteCachesToReadOnly();
	}

	void RenderPathGraph::ResizeBuffers()
	{
		WSIPlatform* platform = wsi->GetPlatform();
		currentBufferSize = U32x2(
			platform->GetWidth(),
			platform->GetHeight()
		);

		GPU::DeviceVulkan* device = wsi->GetDevice();
		renderGraph.Reset();
		renderGraph.SetDevice(device);

		// Setup backbuffer resource dimension
		ResourceDimensions dim;
		dim.width = currentBufferSize.x;
		dim.height = currentBufferSize.y;
		dim.format = wsi->GetSwapchainFormat();
		renderGraph.SetBackbufferDimension(dim);

		AttachmentInfo backInfo;
		backInfo.format = dim.format;
		backInfo.sizeX = (F32)dim.width;
		backInfo.sizeY = (F32)dim.height;

		// Setup render graph
		outputColors.clear();
		Setup(renderGraph);
		
		// Compose
		auto& composePass = renderGraph.AddRenderPass("Compose", RenderGraphQueueFlag::Graphics);
		composePass.WriteColor("back", backInfo);
		for(const auto& color : outputColors)
			composePass.ReadTexture(color.c_str());
		composePass.SetClearColorCallback([](U32 index, VkClearColorValue* value) {
			if (value != nullptr)
			{
				value->float32[0] = 0.0f;
				value->float32[1] = 0.0f;
				value->float32[2] = 0.0f;
				value->float32[3] = 1.0f;
			}
			return true;
		});
		composePass.SetBuildCallback([&](GPU::CommandList& cmd) {
			Compose(renderGraph, &cmd);
		});
		renderGraph.SetBackBufferSource("back");
		renderGraph.Bake();
		renderGraph.Log();
	}
}