#include "renderer.h"
#include "renderer\renderScene.h"
#include "renderer\renderPath3D.h"
#include "gpu\vulkan\wsi.h"
#include "core\utils\profiler.h"

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
		Renderer::Initialize();

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

			// Compose
			Profiler::BeginBlock("RendererCompose");
			GPU::DeviceVulkan* device = engine.GetWSI().GetDevice();
			ASSERT(device != nullptr);
			auto cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);
			activePath->Compose(cmd.get());
			device->Submit(cmd);
			Profiler::EndBlock();
		}
	}

	const char* GetName() const override
	{
		return "Renderer";
	}

	void CreateScene(World& world) override {
		UniquePtr<RenderScene> scene = RenderScene::CreateScene(engine, world);
		world.AddScene(scene.Move());
	}

	void ActivePath(RenderPath* renderPath) override
	{
		activePath = renderPath;
		activePath->SetDevice(engine.GetWSI().GetDevice());
		activePath->SetPlatform(engine.GetWSI().GetPlatform());
	}

	RenderPath* GetActivePath()
	{
		return activePath;
	}

private:
	Engine& engine;
	RenderPath* activePath = nullptr;
	RenderPath3D defaultPath;
};

namespace Renderer
{
	RendererPlugin* CreatePlugin(Engine& engine)
	{
		return CJING_NEW(RendererPluginImpl)(engine);
	}

	void Renderer::Initialize()
	{
		Logger::Info("Render initialized");
	}

	void Renderer::Uninitialize()
	{
		Logger::Info("Render uninitialized");
	}
}
}
