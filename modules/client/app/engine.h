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
			Span<const char*> plugins;
			const char* windowTitle = "VULKAN_TEST";
		};

		virtual ~Engine() {}

		static UniquePtr<Engine> Create(const InitConfig& initConfig);
	};
}