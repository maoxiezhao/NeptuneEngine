#pragma once

#include "editor\common.h"
#include "core\scripts\luaUtils.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	class VULKAN_EDITOR_API Settings
	{
	public:
		explicit Settings(EditorApp& app_);
		~Settings();

		bool Load();
		bool Save();

		struct Rect
		{
			int x, y;
			int w, h;
		};

		Rect window;

		String imguiState;

	private:
		EditorApp& app;
		lua_State* globalState;
	};
}
} 
