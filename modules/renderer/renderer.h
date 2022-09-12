#pragma once

#include "rendererCommon.h"
#include "enums.h"
#include "renderGraph.h"
#include "gpu\vulkan\device.h"
#include "core\plugin\plugin.h"
#include "renderScene.h"
#include "culling.h"
#include "content\resources\shader.h"
#include "content\resources\mesh.h"

namespace VulkanTest
{
	class RenderPath;
	class RenderScene;

	struct VULKAN_TEST_API RendererPlugin : public IPlugin
	{
		virtual GPU::DeviceVulkan* GetDevice() = 0;
		virtual RenderScene* GetScene() = 0;
		virtual Engine& GetEngine() = 0;
	};

	namespace Renderer
	{
		void Initialize(Engine& engine);
		void Uninitialize();

		void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta, FrameCB& frameCB);
		void UpdateRenderData(const Visibility& visible, const FrameCB& frameCB, GPU::CommandList& cmd);

		void BindCameraCB(const CameraComponent& camera, GPU::CommandList& cmd);
		void BindFrameCB(GPU::CommandList& cmd);

		Ray GetPickRay(const F32x2& screenPos, const CameraComponent& camera);
		GPU::DeviceVulkan* GetDevice();
		GPU::Shader* PreloadShader(GPU::ShaderStage stage, const char* path, const GPU::ShaderVariantMap& defines = {});
		const GPU::BlendState& GetBlendState(BlendStateTypes type);
		const GPU::RasterizerState& GetRasterizerState(RasterizerStateTypes type);
		const GPU::DepthStencilState& GetDepthStencilState(DepthStencilStateType type);
		ResPtr<Shader> GetShader(ShaderType type);

		void DrawDebugObjects(const RenderScene& scene, const CameraComponent& camera, GPU::CommandList& cmd);
		void DebugDrawBox(const FMat4x4& boxMatrix, const F32x4& color = F32x4(1.0f));

		enum class RenderQueueType
		{
			Opaque = 1 << 0,
			Transparent = 1 << 1,
			All = Opaque | Transparent
		};

		void DrawScene(GPU::CommandList& cmd, const Visibility& vis, RENDERPASS pass);

		I32 ComputeModelLOD(const Model* model, F32x3 eye, F32x3 pos, F32 radius);

		// Visibiliry
		U32x2 GetVisibilityTileCount(const U32x2& resolution);

		// Light culling
		U32x3 GetLightCullingTileCount(const U32x2& resolution);
		
		// Postprocess
		void SetupPostprocessBlurGaussian(RenderGraph& graph, const String& input, String& out, const AttachmentInfo& attchment);
		void PostprocessOutline(GPU::CommandList& cmd, const GPU::ImageView& texture, F32 threshold, F32 thickness, const F32x4& color);

		// Plugin interface
		RendererPlugin* CreatePlugin(Engine& engine);
	}
}