#pragma once

#include "enums.h"
#include "renderGraph.h"
#include "gpu\vulkan\device.h"
#include "core\plugin\plugin.h"
#include "core\utils\profiler.h"
#include "core\collections\array.h"
#include "renderScene.h"
#include "culling.h"
#include "math\math.hpp"

namespace VulkanTest
{
	class RenderPath;
	class RenderScene;

	struct VULKAN_TEST_API RendererPlugin : public IPlugin
	{
		virtual GPU::DeviceVulkan* GetDevice() = 0;
		virtual RenderScene* GetScene() = 0;
	};

	namespace Renderer
	{
		void Initialize(Engine& engine);
		void Uninitialize();

		void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta, FrameCB& frameCB);
		void UpdateRenderData(const Visibility& visible, const FrameCB& frameCB, GPU::CommandList& cmd);

		void BindCameraCB(const CameraComponent& camera, GPU::CommandList& cmd);
		void BindCommonResources(GPU::CommandList& cmd);

		Ray GetPickRay(const F32x2& screenPos, const CameraComponent& camera);

		GPU::DeviceVulkan* GetDevice();

		const GPU::BlendState& GetBlendState(BlendStateTypes type);
		const GPU::RasterizerState& GetRasterizerState(RasterizerStateTypes type);
		const GPU::DepthStencilState& GetDepthStencilState(DepthStencilStateType type);
		const GPU::Shader* GetShader(ShaderType type);
		const GPU::PipelineStateDesc& GetObjectPipelineState(
			RENDERPASS renderPass,
			BlendMode blendMode,
			ObjectDoubleSided doublesided);

		void DrawDebugObjects(const RenderScene& scene, const CameraComponent& camera, GPU::CommandList& cmd);
		void DebugDrawBox(const FMat4x4& boxMatrix, const F32x4& color = F32x4(1.0f));

		enum class RenderQueueType
		{
			Opaque = 1 << 0,
			Transparent = 1 << 1,
			All = Opaque | Transparent
		};

		void DrawScene(GPU::CommandList& cmd, const Visibility& vis, RENDERPASS pass);

		// RenderGraph setup
		void SetupPostprocessBlurGaussian(RenderGraph& graph, const String& input, String& out, const AttachmentInfo& attchment);

		// Plugin interface
		RendererPlugin* CreatePlugin(Engine& engine);
	}
}