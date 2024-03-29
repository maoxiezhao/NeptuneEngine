#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\scripts\luaUtils.h"
#include "core\filesystem\filesystem.h"

namespace VulkanTest
{
	class Engine;

	class EngineService
	{
	public:
		typedef std::vector<EngineService*> ServicesArray; 	// TODO use Array
		static ServicesArray& GetServices();
		static void Sort();

		virtual ~EngineService();

		virtual bool Init(Engine& engine);
		static void OnInit(Engine& engine);

#define DECLARE_ENGINE_SERVICE(result, name) virtual result name(); static void On##name();
		DECLARE_ENGINE_SERVICE(void, FixedUpdate);
		DECLARE_ENGINE_SERVICE(void, Update);
		DECLARE_ENGINE_SERVICE(void, LateUpdate);
		DECLARE_ENGINE_SERVICE(void, BeforeUninit);
		DECLARE_ENGINE_SERVICE(void, Uninit);
#undef DECLARE_ENGINE_SERVICE_EVENT

		const char* name;
		I32 order;

	protected:
		EngineService(const char* name_, I32 order_ = 0);
		bool initialized = false;
	};

	class VULKAN_TEST_API Engine
	{
	public:
		static Engine* Instance;
		virtual ~Engine() {}

		static void RequestExit(I32 exitCode = 0);
		static bool ShouldExit();
		static void InitGlobalPaths(const char* startupPath);

		virtual class World& CreateWorld() = 0;
		virtual void DestroyWorld(World* world) = 0;

		virtual void LoadPlugins() = 0;
		virtual void Start(World* world) {};
		virtual void Update(World* world, F32 dt) {};
		virtual void LateUpdate(World* world) {};
		virtual void FixedUpdate(World* world) {};
		virtual void Stop(World* world) {};

		virtual class InputSystem& GetInputSystem() = 0;
		virtual class PluginManager& GetPluginManager() = 0;
		virtual class TaskGraph& GetTaskGraph() = 0;
		virtual class WSI& GetWSI() = 0;
		virtual lua_State* GetLuaState() = 0;
	};
}