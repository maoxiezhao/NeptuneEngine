#include "core\engine.h"
#include "core\commandLine.h"
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
		App& app;
		UniquePtr<InputSystem> inputSystem;
		UniquePtr<PluginManager> pluginManager;
		UniquePtr<TaskGraph> taskGraph;
		WSIPlatform* platform;
		WSI wsi;
		bool isGameRunning = false;
		bool isPaused = false;

		lua_State* luaState = NULL;
		size_t luaAllocated = 0;
		DefaultAllocator luaAllocator;

	public:
		EngineImpl(App& app_) :
			app(app_)
		{
			Engine::Instance = this;
			Logger::Info("Create game engine...");
			
			// Init engine paths
			char startupPath[MAX_PATH_LENGTH];
#ifdef CJING3D_EDITOR
			if (!CommandLine::options.workingPath.empty())
				memcpy(startupPath, CommandLine::options.workingPath, CommandLine::options.workingPath.size());
			else
				Platform::GetCurrentDir(startupPath);
#else
			Platform::GetCurrentDir(startupPath);
#endif
			InitPaths(startupPath);

			// Init platform
			platform = &app.GetPlatform();
			SetupUnhandledExceptionHandler();

			// Init task graph
			taskGraph = CJING_MAKE_UNIQUE<TaskGraph>();

			// Init lua system
			luaState = luaL_newstate();
			luaL_openlibs(luaState);

			// Init wsi
			wsi.SetPlatform(&app.GetPlatform());
			I32 wsiThreadCount = std::clamp((I32)(Platform::GetCPUsCount() * 0.5f), 1, 12);
			bool ret = wsi.Initialize(wsiThreadCount);
			ASSERT(ret);

			// Init engine services
			EngineService::OnInit(*this);

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
			platform = nullptr;
			Engine::Instance = nullptr;

			Logger::Info("Game engine released.");
		}

		void InitPaths(const char* startupPath)
		{
			Globals::StartupFolder = startupPath;
#ifdef CJING3D_EDITOR
			Globals::EngineContentFolder = Globals::StartupFolder / "content";
			Globals::ProjectCacheFolder = Globals::ProjectFolder / "cache";
#endif
			Globals::ProjectContentFolder = Globals::ProjectFolder / "content";
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

#if 0
			for (const char* plugin : initConfig.plugins)
			{
				if (plugin == nullptr && !pluginManager->LoadPlugin(plugin))
					Logger::Info("Failed to load plugin:%s", plugin);
			}
#endif

			pluginManager->InitPlugins();
			
			Logger::Info("Engine plugins loaded.");
		}

		World& CreateWorld() override
		{
			World* world = CJING_NEW(World)();
			const auto& plugins = pluginManager->GetPlugins();
			
			// Create scenes from plugins
			for (auto& plugin : plugins)
				plugin->CreateScene(*world);

			// Init scenes
			for (auto& scene : world->GetScenes())
				scene->Init();

			return *world;
		}

		void DestroyWorld(World* world)
		{
			if (world != nullptr)
			{
				auto& scenes = world->GetScenes();
				for (auto& scene : scenes)
					scene->Uninit();
				scenes.clear();

				CJING_DELETE(world);
			}
		}

		void Start(World* world) override
		{
			ASSERT(isGameRunning == false);
			isGameRunning = true;

			for (auto plugin : pluginManager->GetPlugins())
				plugin->OnGameStart();
		}

		void FixedUpdate(World* world) override
		{
			pluginManager->FixedUpdatePlugins();
			EngineService::OnFixedUpdate();
		}

		void Update(World* world, F32 dt) override
		{
			if (world != nullptr)
			{
				PROFILE_BLOCK("Update scenes");
				for (auto& scene : world->GetScenes())
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

		void LateUpdate(World* world)override
		{
			EngineService::OnLateUpdate();
		}

		void Stop(World* world) override
		{
			ASSERT(isGameRunning == true);
			isGameRunning = false;

			// TODO: Plugin is already disposed. There is a conflict between EngineServies and Plugins
			if (pluginManager)
			{
				for (auto plugin : pluginManager->GetPlugins())
					plugin->OnGameStop();
			}
		}

		InputSystem& GetInputSystem() override
		{
			return *inputSystem.Get();
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

	UniquePtr<Engine> CreateEngine(App& app)
	{
		return CJING_MAKE_UNIQUE<EngineImpl>(app);
	}
}