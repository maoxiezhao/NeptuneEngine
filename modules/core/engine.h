#pragma once

#include "core\common.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Engine
	{
	public:
		struct InitConfig
		{
			const char* workingDir = nullptr;
			const char* windowTitle = "VULKAN_TEST";
			Span<const char*> plugins;
		};

		virtual ~Engine() {}

		virtual class World& CreateWorld() = 0;
		virtual void DestroyWorld(World& world) = 0;

		virtual void Start(World& world) {};
		virtual void Update(World& world, F32 dt) {};
		virtual void FixedUpdate(World& world) {};
		virtual void Stop(World& world) {};

		virtual class FileSystem& GetFileSystem() = 0;
		virtual class PluginManager& GetPluginManager() = 0;
	};
}