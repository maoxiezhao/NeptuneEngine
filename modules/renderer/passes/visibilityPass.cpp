#include "visibilityPass.h"
#include "renderer.h"
#include "renderPath3D.h"

namespace VulkanTest
{
	void VisibilityPass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		AttachmentInfo depthAttachment = renderPath.GetAttachmentDepth();
		depthAttachment.format = VK_FORMAT_R32_SFLOAT;

		// Calculate visibility tile count
		U32x2 tileCount = Renderer::GetVisibilityTileCount(U32x2((U32)depthAttachment.sizeX, (U32)depthAttachment.sizeY));

		auto& pass = renderGraph.AddRenderPass("VisbilityPrepare", RenderGraphQueueFlag::Compute);
		pass.ReadTexture("rtPrimitiveID");

		pass.WriteStorageTexture("depthCopy", depthAttachment);
		pass.WriteStorageTexture("depthLinear", depthAttachment);
		pass.SetBuildCallback([&, tileCount](GPU::CommandList& cmd) {

			Renderer::BindFrameCB(cmd);
			Renderer::BindCameraCB(*renderPath.camera, cmd);

			cmd.BeginEvent("Visibility resolve");
			{
				auto textureDepthCopy = renderGraph.TryGetPhysicalTexture("depthCopy");
				auto textureDepthLinear = renderGraph.TryGetPhysicalTexture("depthLinear");

				cmd.SetTexture(0, 0, *renderGraph.TryGetPhysicalTexture("rtPrimitiveID"));
				cmd.SetStorageTexture(0, 1, *textureDepthCopy);
				cmd.SetStorageTexture(0, 2, *textureDepthLinear);

				cmd.SetProgram(Renderer::GetShader(SHADERTYPE_VISIBILITY)->GetCS("CS_Resolve"));
				cmd.Dispatch(
					tileCount.x,
					tileCount.y,
					1
				);
			}
		
			cmd.EndEvent();
		});
	}
}
