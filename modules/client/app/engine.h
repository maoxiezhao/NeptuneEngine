#pragma once

#include "core\common.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
	struct EngineInitConfig
	{
		const char* workingDir = nullptr;
		Span<const char*> plugins;
		const char* windowTitle = "VULKAN_TEST";
	};

	class VULKAN_TEST_API Engine
	{
	public:
		virtual ~Engine() {}

		static UniquePtr<Engine> Create(const EngineInitConfig& initConfig);
	};
}