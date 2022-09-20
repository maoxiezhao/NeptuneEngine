#include "core\engine.h"
#include "core\platform\platform.h"
#include "core\platform\debug.h"
#include "core\scene\world.h"
#include "core\input\inputSystem.h"
#include "core\filesystem\filesystem.h"
#include "content\resourceManager.h"
#include "core\profiler\profiler.h"
#include "core\threading\taskGraph.h"
#include "app\app.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
	class EngineImpl final : public Engine
	{
	private:
		const InitConfig& initConfig;
		App& app;
		UniquePtr<InputSystem> inputSystem;
		UniquePtr<PluginManager> pluginManager;
		UniquePtr<FileSystem> fileSystem;
		UniquePtr<TaskGraph> taskGraph;
		WSIPlatform* platform;
		WSI wsi;
		bool isGameRunning = false;
		bool isPaused = false;

		lua_State* luaState = NULL;
		size_t luaAllocated = 0;
		DefaultAllocator luaAllocator;

	public:
		EngineImpl(const InitConfig& initConfig_, App& app_) :
			initConfig(initConfig_),
			app(app_)
		{
			Logger::Info("Create game engine...");

			platform = &app.GetPlatform();
			SetupUnhandledExceptionHandler();

			// Init task graph
			taskGraph = CJING_MAKE_UNIQUE<TaskGraph>();

			// Init lua system
			luaState = luaL_newstate();
			luaL_openlibs(luaState);

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

			// Init engine services
			EngineService::OnInit(*this);

			// Init wsi
			wsi.SetPlatform(&app.GetPlatform());
			GPU::SystemHandles systemHandles;
			systemHandles.fileSystem = &GetFileSystem();
			I32 wsiThreadCount = std::clamp((I32)(Platform::GetCPUsCount() * 0.5f), 1, 12);
			bool ret = wsi.Initialize(systemHandles, wsiThreadCount);
			ASSERT(ret);

			// Init input system
			inputSystem = InputSystem::Create(*this);

			// Create pluginManager
			pluginManager = PluginManager::Create(*this);

			Logger::Info("Game engine created.");
		}

		virtual ~EngineImpl()
		{
			EngineService::OnBeforeUninit();

			pluginManager.Reset();
			inputSystem.Reset();

			EngineService::OnUninit();
			taskGraph.Reset();

			lua_close(luaState);

			wsi.Uninitialize();
			fileSystem.Reset();
			platform = nullptr;

			Logger::Info("Game engine released.");
		}

		void LoadPlugins()override
		{
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
			
			Logger::Info("Engine plugins loaded.");
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
			EngineService::OnFixedUpdate();
		}

		void Update(World& world, F32 dt) override
		{
			{
				PROFILE_BLOCK("Update scenes");
				for (auto& scene : world.GetScenes())
					scene->Update(dt, isPaused);
			}
			pluginManager->UpdatePlugins(dt);

			// Update input system
			inputSystem->Update(dt);

			// Update task graph
			taskGraph->Execute();

			// Update engine servies
			EngineService::OnUpdate();
		}

		void LateUpdate(World& world)override
		{
			EngineService::OnLateUpdate();
		}

		void Stop(World& world) override
		{
			ASSERT(isGameRunning == true);
			isGameRunning = false;

			for (auto plugin : pluginManager->GetPlugins())
				plugin->OnGameStop();
		}

		InputSystem& GetInputSystem() override
		{
			return *inputSystem.Get();
		}

		FileSystem& GetFileSystem() override
		{
			return *fileSystem.Get();
		}

		PluginManager& GetPluginManager() override
		{
			return *pluginManager.Get();
		}

		TaskGraph& GetTaskGraph() override
		{
			return *taskGraph.Get();
		}

		WSI& GetWSI() override
		{
			return wsi;
		}

		lua_State* GetLuaState() override
		{
			return luaState;
		}
	};

	UniquePtr<Engine> CreateEngine(const Engine::InitConfig& config, App& app)
	{
		return CJING_MAKE_UNIQUE<EngineImpl>(config, app);
	}
}