#pragma once

#include "editor\common.h"

struct ImFont;

namespace VulkanTest
{
namespace Editor::Utils
{
	struct EditorCommand
	{
		virtual ~EditorCommand() {}

		virtual bool DoCommand() = 0;
		virtual void UndoCommand() = 0;
		virtual const char* GetType() = 0;
		virtual bool Merge(EditorCommand& command) = 0;
	};

	struct VULKAN_EDITOR_API Action
	{
		Action();

		void Init(const char* label_, const char* name_);
		void Init(const char* label_, const char* name_, const char* tooltip_, const char* icon_);

		static bool FalseConst() { return false; }

		StaticString<32> label;
		StaticString<32> name;
		Platform::Keycode shortcut;
		Delegate<void()> func;
		Delegate<bool()> isSelected;
		StaticString<8> icon;
		StaticString<32> tooltip;

		bool IsActive();
		bool ToolbarButton(ImFont* font);
	};
}
}