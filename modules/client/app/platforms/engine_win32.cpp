#include "core\engine.h"
#include "core\platform\platform.h"
#include "core\platform\debug.h"
#include "core\scene\world.h"
#include "core\filesystem\filesystem.h"
#include "core\resource\resourceManager.h"
#include "core\utils\profiler.h"
#include "app\app.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
	class EngineImpl final : public Engine
	{
	private:
		UniquePtr<PluginManager> pluginManager;
		UniquePtr<FileSystem> fileSystem;
		ResourceManager resourceManager;
		WSIPlatform* platform;
		WSI* wsi;
		bool isGameRunning = false;
		bool isPaused = false;
		F32 lastTimeDeltas[8] = {};
		U32 lastTimeFrames = 0;

	public:
		EngineImpl(const InitConfig& initConfig, App& app)
		{
			Logger::Info("Create game engine...");

			Platform::Initialize();
			Platform::LogPlatformInfo();
			platform = &app.GetPlatform();
			wsi = &app.GetWSI();
			SetupUnhandledExceptionHandler();

			// Create filesystem
			if (initConfig.workingDir != nullptr) {
				fileSystem = FileSystem::Create(initConfig.workingDir);
			}
			else
			{
				char currentDir[MAX_PATH_LENGTH];
				Platform::GetCurrentDir(currentDir);
				fileSystem = FileSystem::Create(currentDir);
			}

			// Init resource manager
			resourceManager.Initialize(*fileSystem);

			// Create pluginManager
			pluginManager = PluginManager::Create(*this);

			// Builtin plugins
			pluginManager->AddPlugin(Renderer::CreatePlugin(*this));

#ifdef VULKAN_TEST_PLUGINS
			const char* plugins[] = { VULKAN_TEST_PLUGINS };
			Span<const char*> pluginSpan = Span(plugins);
			for (const char* plugin : pluginSpan) 
			{
				if (plugin == nullptr && !pluginManager->LoadPlugin(plugin))
					Logger::Info("Failed to load plugin:%s", plugin);
			}
#endif
			for (const char* plugin : initConfig.plugins)
			{
				if (plugin == nullptr && !pluginManager->LoadPlugin(plugin))
					Logger::Info("Failed to load plugin:%s", plugin);
			}

			pluginManager->InitPlugins();

			Logger::Info("Game engine created.");
		}

		virtual ~EngineImpl()
		{
			pluginManager.Reset();
			resourceManager.Uninitialzie();
			fileSystem.Reset();
			platform = nullptr;

			Logger::Info("Game engine released.");
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
			ASSERT(isGameRunning == false);
			isGameRunning = true;

			for (auto plugin : pluginManager->GetPlugins())
				plugin->OnGameStart();
		}

		void FixedUpdate(World& world) override
		{
			pluginManager->FixedUpdatePlugins();
		}

		void Update(World& world, F32 dt) override
		{
			lastTimeDeltas[(lastTimeFrames++) % ARRAYSIZE(lastTimeDeltas)] = dt;
			{
				PROFILE_BLOCK("Update scenes");
				for (auto& scene : world.GetScenes())
					scene->Update(dt, isPaused);
			}
			pluginManager->UpdatePlugins(dt);

			// Process async loading jobs
			fileSystem->ProcessAsync();
		}

		void Stop(World& world) override
		{
			ASSERT(isGameRunning == true);
			isGameRunning = false;

			for (auto plugin : pluginManager->GetPlugins())
				plugin->OnGameStop();
		}

		FileSystem& GetFileSystem() override
		{
			return *fileSystem.Get();
		}

		ResourceManager& GetResourceManager() override
		{
			return resourceManager;
		}

		PluginManager& GetPluginManager() override
		{
			return *pluginManager.Get();
		}

		WSI& GetWSI() override
		{
			return *wsi;
		}
	};

	UniquePtr<Engine> CreateEngine(const Engine::InitConfig& config, App& app)
	{
		return CJING_MAKE_UNIQUE<EngineImpl>(config, app);
	}
}