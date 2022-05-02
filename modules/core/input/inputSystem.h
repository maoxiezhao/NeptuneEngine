#pragma once

#include "core\common.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
	class VULKAN_TEST_API InputSystem
	{
	public:
		static UniquePtr<InputSystem> Create(class Engine& engine_);

		virtual ~InputSystem() = default;
		virtual void Update(F32 dt) = 0;
	};
}