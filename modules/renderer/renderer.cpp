#include "renderer.h"
#include "renderer\renderScene.h"
#include "renderer\renderPath3D.h"
#include "gpu\vulkan\wsi.h"
#include "core\utils\profiler.h"
#include "core\resource\resourceManager.h"
#include "model.h"
#include "material.h"
#include "texture.h"

namespace VulkanTest
{

struct RendererPluginImpl : public RendererPlugin
{
public:
	RendererPluginImpl(Engine& engine_)
		: engine(engine_)
	{
	}

	virtual ~RendererPluginImpl()
	{
		Renderer::Uninitialize();
	}

	void Initialize() override
	{
		Renderer::Initialize(engine);

		// Activate the default render path if no custom path is set
		if (GetActivePath() == nullptr) {
			ActivePath(&defaultPath);
		}
	}
	
	void OnGameStart() override
	{
		if (activePath != nullptr)
			activePath->Start();
	}
	
	void OnGameStop() override
	{
		if (activePath != nullptr)
			activePath->Stop();
	}

	void FixedUpdate() override
	{
		PROFILE_BLOCK("RendererFixedUpdate");
		if (activePath != nullptr)
			activePath->FixedUpdate();
	}

	void Update(F32 delta) override
	{
		PROFILE_BLOCK("RendererUpdate");
		if (activePath != nullptr)
			activePath->Update(delta);
	}

	void Render() override
	{
		if (activePath != nullptr)
		{
			// Render
			Profiler::BeginBlock("RendererRender");
			activePath->Render();
			Profiler::EndBlock();
		}
	}

	const char* GetName() const override
	{
		return "Renderer";
	}

	void CreateScene(World& world) override {
		UniquePtr<RenderScene> newScene = RenderScene::CreateScene(*this, engine, world);
		scene = newScene.Get();
		world.AddScene(newScene.Move());
	}

	void ActivePath(RenderPath* renderPath) override
	{
		activePath = renderPath;
		activePath->SetWSI(&engine.GetWSI());
		activePath->SetScene(scene);
	}

	RenderPath* GetActivePath()
	{
		return activePath;
	}

private:
	Engine& engine;
	RenderPath* activePath = nullptr;
	RenderPath3D defaultPath;
	RenderScene* scene = nullptr;
};

namespace Renderer
{
	template <typename T>
	struct RenderResourceFactory : public ResourceFactory
	{
	protected:
		virtual Resource* CreateResource(const Path& path) override
		{
			return CJING_NEW(T)(path, *this);
		}

		virtual void DestroyResource(Resource* res) override
		{
			CJING_DELETE(res);
		}
	};
	RenderResourceFactory<Texture> textureFactory;
	RenderResourceFactory<Model> modelFactory;

	GPU::BlendState stockBlendStates[BlendStateType_Count] = {};
	GPU::RasterizerState stockRasterizerState[RasterizerStateType_Count] = {};

	RendererPlugin* rendererPlugin = nullptr;

	void InitStockStates()
	{
		// Blend states
		GPU::BlendState bd;
		bd.renderTarget[0].blendEnable = false;
		bd.renderTarget[0].srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
		bd.renderTarget[0].destBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		bd.renderTarget[0].blendOp = VK_BLEND_OP_MAX;
		bd.renderTarget[0].srcBlendAlpha = VK_BLEND_FACTOR_ONE;
		bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_ZERO;
		bd.renderTarget[0].blendOpAlpha = VK_BLEND_OP_ADD;
		bd.renderTarget[0].renderTargetWriteMask = GPU::COLOR_WRITE_ENABLE_ALL;
		bd.alphaToCoverageEnable = false;
		bd.independentBlendEnable = false;
		stockBlendStates[BlendStateType_Opaque] = bd;

		bd.renderTarget[0].blendEnable = true;
		bd.renderTarget[0].srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
		bd.renderTarget[0].destBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		bd.renderTarget[0].blendOp = VK_BLEND_OP_ADD;
		bd.renderTarget[0].srcBlendAlpha = VK_BLEND_FACTOR_ONE;
		bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		bd.renderTarget[0].blendOpAlpha = VK_BLEND_OP_ADD;
		bd.renderTarget[0].renderTargetWriteMask = GPU::COLOR_WRITE_ENABLE_ALL;
		bd.alphaToCoverageEnable = false;
		bd.independentBlendEnable = false;
		stockBlendStates[BlendStateType_Transparent] = bd;

		// Rasterizer states
		GPU::RasterizerState rs;
		rs.fillMode = GPU::FILL_SOLID;
		rs.cullMode = VK_CULL_MODE_BACK_BIT;
		rs.frontCounterClockwise = true;
		rs.depthBias = 0;
		rs.depthBiasClamp = 0;
		rs.slopeScaledDepthBias = 0;
		rs.depthClipEnable = false;
		rs.multisampleEnable = false;
		rs.antialiasedLineEnable = false;
		rs.conservativeRasterizationEnable = false;
		stockRasterizerState[RasterizerStateType_Front] = rs;

		rs.cullMode = VK_CULL_MODE_FRONT_BIT;
		stockRasterizerState[RasterizerStateType_Back] = rs;

		rs.fillMode = GPU::FILL_SOLID;
		rs.cullMode = VK_CULL_MODE_NONE;
		rs.frontCounterClockwise = false;
		rs.depthBias = 0;
		rs.depthBiasClamp = 0;
		rs.slopeScaledDepthBias = 0;
		rs.depthClipEnable = false;
		rs.multisampleEnable = false;
		rs.antialiasedLineEnable = false;
		rs.conservativeRasterizationEnable = false;
		stockRasterizerState[RasterizerStateType_DoubleSided] = rs;
	}

	void Renderer::Initialize(Engine& engine)
	{
		Logger::Info("Render initialized");
		InitStockStates();

		// Initialize resource factories
		ResourceManager& resManager = engine.GetResourceManager();
		textureFactory.Initialize(Texture::ResType, resManager);
		modelFactory.Initialize(Model::ResType, resManager);
	}

	void Renderer::Uninitialize()
	{
		// Uninitialize resource factories
		modelFactory.Uninitialize();
		textureFactory.Uninitialize();

		rendererPlugin = nullptr;
		Logger::Info("Render uninitialized");
	}

	const GPU::BlendState& GetBlendState(BlendStateTypes types)
	{
		return stockBlendStates[types];
	}
	
	const GPU::RasterizerState& GetRasterizerState(RasterizerStateTypes types)
	{
		return stockRasterizerState[types];
	}

	void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta)
	{

	}

	void UpdateRenderData(const Visibility& visible, GPU::CommandList& cmd)
	{

	}

	void DrawScene(GPU::CommandList& cmd)
	{
	}

	void DrawMeshes(GPU::CommandList& cmd, const Visibility& visible, const RenderQueue& queue, RENDERPASS renderPass, U32 renderFlags)
	{
	}

	RendererPlugin* CreatePlugin(Engine& engine)
	{
		rendererPlugin = CJING_NEW(RendererPluginImpl)(engine);
		return rendererPlugin;
	}
}
}
