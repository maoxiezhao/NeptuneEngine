#include "renderer.h"
#include "renderer\renderScene.h"
#include "renderer\renderPath3D.h"
#include "core\utils\profiler.h"

namespace VulkanTest
{

struct RendererPlugin : IPlugin
{
public:
	RendererPlugin(Engine& engine_)
		: engine(engine_)
	{
	}

	virtual ~RendererPlugin()
	{
		Renderer::Uninitialize();
	}

	void Initialize() override
	{
		Renderer::Initialize();

		// Activate the default render path if no custom path is set
		if (GetActivePath() == nullptr) {
			ActivatePath(&defaultPath);
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
		PROFILE_BLOCK("RenderFixedUpdate");
		if (activePath != nullptr)
			activePath->FixedUpdate();
	}

	void Update(F32 delta) override
	{
		PROFILE_BLOCK("RenderUpdate");
		if (activePath != nullptr)
			activePath->Update(delta);
	}

	const char* GetName() const override
	{
		return "Renderer";
	}

	void CreateScene(World& world) override {
		UniquePtr<RenderScene> scene = RenderScene::CreateScene(engine, world);
		world.AddScene(scene.Move());
	}

	void ActivatePath(RenderPath* renderPath)
	{
		activePath = renderPath;
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

IPlugin* Renderer::CreatePlugin(Engine& engine)
{
	return CJING_NEW(RendererPlugin)(engine);
}

void Renderer::Initialize()
{
	Logger::Info("Render initialized");
}

void Renderer::Uninitialize()
{
	Logger::Info("Render uninitialized");
}

void Renderer::ActiveRenderPath(Engine& engine, RenderPath* renderPath)
{
	RendererPlugin* plugin = static_cast<RendererPlugin*>(engine.GetPluginManager().GetPlugin("Renderer"));
	ASSERT(plugin != nullptr);
	plugin->ActivatePath(renderPath);
}
}
