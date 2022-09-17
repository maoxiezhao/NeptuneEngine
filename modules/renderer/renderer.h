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
		virtual RenderScene* GetScene() = 0;
		virtual Engine& GetEngine() = 0;
	};

	// Use RendererService to replace the EngineService, manage the lifetime in the renderer module
	class RendererService
	{
	public:
		typedef std::vector<RendererService*> RenderingServicesArray; 	// TODO use Array
		static RenderingServicesArray& GetRenderingServices();
		static void Sort();

		virtual ~RendererService();

		virtual bool Init(Engine& engine);
		virtual void Uninit();

		static void OnInit(Engine& engine);
		static void OnUninit();

		const char* name;
		I32 order;

	protected:
		RendererService(const char* name_, I32 order_ = 0);
		bool initialized = false;
	};

	namespace Renderer
	{
		void Initialize(Engine& engine);
		void Uninitialize();

		void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta, FrameCB& frameCB);
		void UpdateRenderData(const Visibility& visible, const FrameCB& frameCB, GPU::CommandList& cmd);

		void BindCameraCB(const CameraComponent& camera, GPU::CommandList& cmd);
		void BindFrameCB(GPU::CommandList& cmd);

		const GPU::BlendState& GetBlendState(BlendStateTypes type);
		const GPU::RasterizerState& GetRasterizerState(RasterizerStateTypes type);
		const GPU::DepthStencilState& GetDepthStencilState(DepthStencilStateType type);
		ResPtr<Shader> GetShader(ShaderType type);

		// Scene
		Ray GetPickRay(const F32x2& screenPos, const CameraComponent& camera);
		void DrawScene(const Visibility& vis, RENDERPASS pass, GPU::CommandList& cmd);
		void DrawSky(RenderScene& scene, GPU::CommandList& cmd);
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