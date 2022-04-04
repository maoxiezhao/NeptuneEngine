#include "core\engine.h"
#include "core\scene\world.h"
#include "app\app.h"

namespace VulkanTest
{
	class EngineImpl final : public Engine
	{
	private:
		UniquePtr<PluginManager> pluginManager;

	public:
		EngineImpl(const InitConfig& initConfig)
		{
			pluginManager = PluginManager::Create(*this);
		}

		virtual ~EngineImpl()
		{
			pluginManager.Reset();
		}

		World& CreateWorld() override
		{
			World* world = CJING_NEW(World)(this);
			const auto& plugins = pluginManager->GetPlugins();
			
			// Create scenes from plugins
			for (auto& plugin : plugins)
				plugin->CreateScene(*world);

			// Init scenes
			for (auto& scene : world->GetScenes())
				scene->Init();

			return *world;
		}

		void DestroyWorld(World& world)
		{
			// Uninit scenes
			auto& scenes = world.GetScenes();
			for (auto& scene : scenes)
				scene->Uninit();
			scenes.clear();

			CJING_DELETE(&world);
		}

		void Start(World& world) override
		{
		}

		void Update(World& world, F32 dt) override
		{
		}

		void Stop(World& world) override
		{
		}
	};

	UniquePtr<Engine> CreateEngine(const Engine::InitConfig& config)
	{
		return CJING_MAKE_UNIQUE<EngineImpl>(config);
	}
}