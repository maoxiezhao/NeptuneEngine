#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\scene\world.h"

namespace VulkanTest
{
	class Engine;

	class VULKAN_TEST_API IPlugin
	{
	public:
		virtual ~IPlugin() = default;

		virtual void Initialize() {}
		virtual void FixedUpdate() {}
		virtual void Update(F32 delta) {}
		virtual void OnAdded(IPlugin& plugin) {}
		virtual void OnGameStart() {}
		virtual void OnGameStop() {}
		virtual void CreateScene(World& world) {}
		virtual const char* GetName() const = 0;
	};

	class VULKAN_TEST_API PluginManager
	{
	public:
		static UniquePtr<PluginManager> Create(Engine& engine);

		virtual ~PluginManager() = default;

		virtual void AddPlugin(IPlugin* plugin) = 0;
		virtual IPlugin* LoadPlugin(const char* name) = 0;
		virtual IPlugin* GetPlugin(const char* name) = 0;
		virtual const std::vector<IPlugin*>& GetPlugins()const = 0;

		virtual void InitPlugins() = 0;
		virtual void UpdatePlugins(F32 deltaTime) = 0;
		virtual void FixedUpdatePlugins() = 0;
	};
}