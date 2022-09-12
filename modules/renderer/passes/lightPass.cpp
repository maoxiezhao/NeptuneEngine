#include "lightPass.h"
#include "renderer.h"
#include "renderPath3D.h"
#include "renderer.h"
#include "renderPath3D.h"

namespace VulkanTest
{
	void LightPass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		AttachmentInfo rtAttachment = renderPath.GetAttachmentRT();
		U32x3 tileCount = Renderer::GetLightCullingTileCount(U32x2((U32)rtAttachment.sizeX, (U32)rtAttachment.sizeY));

		// Frustum computation
		auto& pass = renderGraph.AddRenderPass("FrustumComputation", RenderGraphQueueFlag::AsyncCompute);
		pass.ReadTexture("depthCopy");

		BufferInfo frustumBufferInfo = {};
		frustumBufferInfo.size = (sizeof(F32x4) * 4) * tileCount.x * tileCount.y;
		auto& frustumRes = pass.WriteStorageBuffer("TiledFrustum", frustumBufferInfo);

		pass.SetBuildCallback([&, tileCount](GPU::CommandList& cmd) {
			Renderer::BindCameraCB(*renderPath.camera, cmd);
			cmd.BeginEvent("Tile Frustums");
			cmd.SetStorageBuffer(0, 0, renderGraph.GetPhysicalBuffer(frustumRes));
			cmd.SetProgram(Renderer::GetShader(SHADERTYPE_TILED_LIGHT_CULLING)->GetCS("CS_Frustum"));
			cmd.Dispatch(
				(tileCount.x + TILED_CULLING_BLOCK_SIZE - 1) / TILED_CULLING_BLOCK_SIZE,
				(tileCount.y + TILED_CULLING_BLOCK_SIZE - 1) / TILED_CULLING_BLOCK_SIZE,
				1
			);
			cmd.EndEvent();
		});

		// Light culling
		auto& cullingPass = renderGraph.AddRenderPass("LightCulling", RenderGraphQueueFlag::AsyncCompute);
		cullingPass.ReadStorageBufferReadonly("TiledFrustum");

		BufferInfo lightTileBufferInfo = {};
		lightTileBufferInfo.size = tileCount.x * tileCount.y * sizeof(U32) * SHADER_ENTITY_TILE_BUCKET_COUNT;
		auto& tilesRes = cullingPass.WriteStorageBuffer("lightTiles", lightTileBufferInfo);

		RenderTextureResource* debugRes = nullptr;
		bool debugLightCulling = true;
		if (debugLightCulling)
		{
			AttachmentInfo debugInfo = rtAttachment;
			debugInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			debugRes = &cullingPass.WriteStorageTexture("DebugCulling", debugInfo);
		}

		cullingPass.SetBuildCallback([&, tileCount, debugLightCulling, debugRes](GPU::CommandList& cmd) {

			Renderer::BindFrameCB(cmd);
			Renderer::BindCameraCB(*renderPath.camera, cmd);
			
			cmd.BeginEvent("Light culling");
			cmd.SetStorageBuffer(0, 0, renderGraph.GetPhysicalBuffer(frustumRes));
			cmd.SetStorageBuffer(0, 1, renderGraph.GetPhysicalBuffer(tilesRes));

			if (debugLightCulling)
				cmd.SetStorageTexture(0, 2, renderGraph.GetPhysicalTexture(*debugRes));

			cmd.SetProgram(Renderer::GetShader(SHADERTYPE_TILED_LIGHT_CULLING)->GetCS("CS_LightCulling", debugLightCulling ? 1 : 0));
			cmd.Dispatch(tileCount.x, tileCount.y, 1);
			cmd.EndEvent();
		});
	}
}
