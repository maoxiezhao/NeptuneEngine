#pragma once

#include "editor\common.h"
#include "core\scripts\luaUtils.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	class VULKAN_EDITOR_API EditorStyle
	{
	public:
		explicit EditorStyle(EditorApp& app_);
		~EditorStyle();

		void Load(lua_State* l);
		void Save(File& file);
		void OnGUI();

	private:
		void ShowStyleEditor();

	private:
		EditorApp& app;
	};
}
}
