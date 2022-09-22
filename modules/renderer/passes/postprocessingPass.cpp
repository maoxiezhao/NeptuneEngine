#include "postprocessingPass.h"
#include "renderer.h"
#include "renderPath3D.h"
#include "renderer.h"
#include "renderPath3D.h"
#include "shaderInterop_postprocess.h"

namespace VulkanTest
{
	String PostprocessingPass::RtPostprocess = "";
	String rtTonemap = "rtTonemap";

	void PostprocessingPass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		AttachmentInfo rtAttachment = renderPath.GetAttachmentRT();
		{
			auto& pass = renderGraph.AddRenderPass("PostprocessTonemap", RenderGraphQueueFlag::Compute);
			auto& inputTex = pass.ReadTexture(renderPath.GetRenderResult3D());
			auto& outptuTex = pass.WriteStorageTexture("rtTonemap", rtAttachment);
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
					(U32)(rtAttachment.sizeX + POSTPROCESS_BLOCKSIZE - 1) / POSTPROCESS_BLOCKSIZE,
					(U32)(rtAttachment.sizeY + POSTPROCESS_BLOCKSIZE - 1) / POSTPROCESS_BLOCKSIZE,
					1
				);
				cmd.EndEvent();
			});

			RtPostprocess = "rtTonemap";
		}
		
		if (renderPath.GetFXAAEnabled())
		{
			auto& pass = renderGraph.AddRenderPass("PostprocessFXAA", RenderGraphQueueFlag::Compute);
			auto& inputTex = pass.ReadTexture(RtPostprocess);
			auto& outptuTex = pass.WriteStorageTexture("rtFXAA", rtAttachment);
			pass.SetBuildCallback([&, rtAttachment](GPU::CommandList& cmd) {

				cmd.BeginEvent("PostprocessFXAA");
				auto& textureInput = renderGraph.GetPhysicalTexture(inputTex);
				auto& textureOutput = renderGraph.GetPhysicalTexture(outptuTex);
				cmd.SetTexture(0, 0, textureInput);
				cmd.SetStorageTexture(0, 0, textureOutput);
				cmd.SetProgram(Renderer::GetShader(SHADERTYPE_FXAA)->GetCS("CS"));

				PostprocessPushConstants push;
				push.resolution = { (U32)rtAttachment.sizeX, (U32)rtAttachment.sizeY };
				push.resolution_rcp = {
					1.0f / rtAttachment.sizeX,
					1.0f / rtAttachment.sizeY,
				};
				cmd.PushConstants(&push, 0, sizeof(push));

				cmd.Dispatch(
					(U32)(rtAttachment.sizeX + POSTPROCESS_BLOCKSIZE - 1) / POSTPROCESS_BLOCKSIZE,
					(U32)(rtAttachment.sizeY + POSTPROCESS_BLOCKSIZE - 1) / POSTPROCESS_BLOCKSIZE,
					1
				);
				cmd.EndEvent();
			});

			RtPostprocess = "rtFXAA";
		}
		
		if (!RtPostprocess.empty())
			renderPath.SetRenderResult3D(RtPostprocess);
	}
}
