#pragma once

#include "core\common.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
	class App;

	class VULKAN_TEST_API Engine
	{
	public:
		virtual ~Engine() {}

		static UniquePtr<Engine> Create(App& app);
	};
}