#include "renderPath3D.h"

namespace VulkanTest
{
	void RenderPath3D::Update(float dt)
	{
		RenderPath2D::Update(dt);
	}

	void RenderPath3D::FixedUpdate()
	{
		RenderPath2D::FixedUpdate();
	}

	void RenderPath3D::Setup(RenderGraph& renderGraph)
	{
		AttachmentInfo color;
		color.format = VK_FORMAT_R8G8B8A8_SRGB;
		color.sizeX = 1.0f;
		color.sizeY = 1.0f;
		color.samples = VK_SAMPLE_COUNT_1_BIT;

		auto& pass3D = renderGraph.AddRenderPass("Pass3D", RenderGraphQueueFlag::Graphics);
		pass3D.WriteColor("rtFinal3D", color);
		pass3D.SetClearColorCallback([](U32 index, VkClearColorValue* value) {
			if (value != nullptr)
			{
				value->float32[0] = 0.0f;
				value->float32[1] = 0.0f;
				value->float32[2] = 0.0f;
				value->float32[3] = 1.0f;
			}
			return true;
		});
		pass3D.SetBuildCallback([&](GPU::CommandList& cmd) {
			cmd.SetDefaultOpaqueState();
			cmd.SetProgram("screenVS.hlsl", "screenPS.hlsl");
			cmd.Draw(3);
		});
		AddOutputColor("rtFinal3D");
		RenderPath2D::Setup(renderGraph);
	}

	void RenderPath3D::Compose(RenderGraph& renderGraph, GPU::CommandList* cmd)
	{
		RenderTextureResource rtFinal = renderGraph.GetOrCreateTexture("rtFinal3D");
		GPU::ImageView& color = renderGraph.GetPhysicalTexture(rtFinal);

		cmd->SetDefaultOpaqueState();
		cmd->SetProgram("test/blitVS.hlsl", "test/blitPS.hlsl");
		cmd->SetSampler(0, 0, GPU::StockSampler::NearestClamp);
		cmd->SetTexture(1, 0, color);
		cmd->Draw(3);

		RenderPath2D::Compose(renderGraph, cmd);
	}
}