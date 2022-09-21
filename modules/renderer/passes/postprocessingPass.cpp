#include "postprocessingPass.h"
#include "renderer.h"
#include "renderPath3D.h"
#include "renderer.h"
#include "renderPath3D.h"
#include "shaderInterop_postprocess.h"

namespace VulkanTest
{
	const String PostprocessingPass::RtPostprocess = "rtPostprocess";

	void PostprocessingPass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		AttachmentInfo rtAttachment = renderPath.GetAttachmentRT();
		auto& pass = renderGraph.AddRenderPass("PostprocessChain", RenderGraphQueueFlag::Compute);
		auto& inputTex = pass.ReadTexture(renderPath.GetRenderResult3D());	
		auto& outptuTex = pass.WriteStorageTexture(RtPostprocess, rtAttachment);
		pass.SetBuildCallback([&, rtAttachment](GPU::CommandList& cmd) {

			cmd.BeginEvent("PostprocessTonemap");
			auto& textureInput = renderGraph.GetPhysicalTexture(inputTex);
			auto& textureOutput = renderGraph.GetPhysicalTexture(outptuTex);
			cmd.SetTexture(0, 0, textureInput);
			cmd.SetStorageTexture(0, 0, textureOutput);
			cmd.SetProgram(Renderer::GetShader(SHADERTYPE_TONEMAP)->GetCS("CS"));

			TonemapPushConstants push;
			push.resolution_rcp = {
				1.0f / rtAttachment.sizeX,
				1.0f / rtAttachment.sizeY,
			};
			push.exposure = renderPath.GetExposure();
			push.dither = 0.0f;
			cmd.PushConstants(&push, 0, sizeof(push));

			cmd.Dispatch(
				(rtAttachment.sizeX + POSTPROCESS_BLOCKSIZE - 1) / POSTPROCESS_BLOCKSIZE,
				(rtAttachment.sizeY + POSTPROCESS_BLOCKSIZE - 1) / POSTPROCESS_BLOCKSIZE,
				1
			);
			cmd.EndEvent();
		});

		renderPath.SetRenderResult3D(RtPostprocess);
	}
}
