#include "visibilityPass.h"
#include "renderer.h"
#include "renderPath3D.h"

namespace VulkanTest
{
	const String VisibilityPass::DepthCopy = "depthCopy";
	const String VisibilityPass::DepthLinear = "depthLinear";

	bool VisibilityPass::Init()
	{
		shader = ResourceManager::LoadResourceInternal<Shader>(Path("shaders/visibility"));
		return true;
	}

	void VisibilityPass::Dispose()
	{
		shader.reset();
	}

	void VisibilityPass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		// Calculate visibility tile count
		AttachmentInfo depthAttachment = renderPath.GetAttachmentDepth();
		depthAttachment.format = VK_FORMAT_R32_SFLOAT;
		U32x2 tileCount = Renderer::GetVisibilityTileCount(U32x2((U32)depthAttachment.sizeX, (U32)depthAttachment.sizeY));

		auto& pass = renderGraph.AddRenderPass("VisbilityPrepare", RenderGraphQueueFlag::Compute);
		pass.ReadTexture("rtPrimitiveID");

		pass.WriteStorageTexture(DepthCopy, depthAttachment);
		pass.WriteStorageTexture(DepthLinear, depthAttachment);
		pass.SetBuildCallback([&, tileCount](GPU::CommandList& cmd) {

			Renderer::BindFrameCB(cmd);
			Renderer::BindCameraCB(*renderPath.camera, cmd);

			cmd.BeginEvent("Visibility resolve");
			{
				auto textureDepthCopy = renderGraph.TryGetPhysicalTexture(DepthCopy);
				auto textureDepthLinear = renderGraph.TryGetPhysicalTexture(DepthLinear);

				cmd.SetTexture(0, 0, *renderGraph.TryGetPhysicalTexture("rtPrimitiveID"));
				cmd.SetStorageTexture(0, 1, *textureDepthCopy);
				cmd.SetStorageTexture(0, 2, *textureDepthLinear);

				cmd.SetProgram(shader->GetCS("CS_Resolve"));
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
