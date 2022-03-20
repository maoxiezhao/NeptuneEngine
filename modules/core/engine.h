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
	};

	static UniquePtr<Engine> Create(const Engine::InitConfig& config);
}