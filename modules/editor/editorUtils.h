#pragma once

#include "editor\common.h"

namespace VulkanTest
{
namespace Editor::Utils
{
	struct VULKAN_EDITOR_API Action
	{
		Action(const char* label_, const char* name_);

		static bool FalseConst() { return false; }

		StaticString<32> label;
		StaticString<32> name;
		Delegate<void()> func;
		Delegate<bool()> isSelected;
	};
}
}