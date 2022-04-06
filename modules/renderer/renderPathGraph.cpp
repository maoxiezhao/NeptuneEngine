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
		

		RenderPath::Update(dt);
	}

	void RenderPathGraph::Render()
	{
		renderGraph.SetupAttachments(*device, &device->GetSwapchainView());
		Jobsystem::JobHandle handle = Jobsystem::INVALID_HANDLE;
		renderGraph.Render(*device, handle);
		Jobsystem::Wait(handle);

		device->MoveReadWriteCachesToReadOnly();
	}

	void RenderPathGraph::ResizeBuffers()
	{
		renderGraph.Reset();
		renderGraph.SetDevice(device);
		SetupRenderGraph(renderGraph);
	}
}