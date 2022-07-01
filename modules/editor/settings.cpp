#include "settings.h"
#include "editor.h"
#include "core\filesystem\filesystem.h"
#include "core\scripts\luaUtils.h"

namespace VulkanTest
{
namespace Editor
{
	// TODO: use ScriptConfig to replace hands-write codes

	static const char SETTINGS_PATH[] = "editor.ini";

	Settings::Settings(EditorApp& app_) :
		app(app_)
	{
		window.x = window.y = 0;
		window.w = window.h = -1;

		globalState = luaL_newstate();
		luaL_openlibs(globalState);
		lua_newtable(globalState);
		lua_setglobal(globalState, "custom");
	}

	Settings::~Settings()
	{
		lua_close(globalState);
	}

	bool Settings::Load()
	{
		lua_State* l = globalState;
		FileSystem& fs = app.GetEngine().GetFileSystem();
		if (!fs.FileExists(SETTINGS_PATH))
		{
			Logger::Error("Failed to open %s", SETTINGS_PATH);
			return false;
		}

		OutputMemoryStream buf;
		if (!fs.LoadContext(SETTINGS_PATH, buf))
		{
			Logger::Error("Failed to open %s", SETTINGS_PATH);
			return false;
		}

		Span<const char> content((const char*)buf.Data(), (U32)buf.Size());
		if (!LuaUtils::Execute(l, content, "settings", 0))
			return false;
		
		// Check the version
		bool validVersion = false;
		lua_getglobal(l, "version");
		if (LuaUtils::Get<int>(l, -1) == 1)
			validVersion = true;
		lua_pop(l, 1);

		if (validVersion == false)
		{
			Logger::Error("Failed to open %s", SETTINGS_PATH);
			return false;
		}

		// Load window settings
		lua_getglobal(l, "window");
		if (lua_type(l, -1) == LUA_TTABLE)
		{
			window.x = LuaUtils::GetField<I32>(l, "x");
			window.y = LuaUtils::GetField<I32>(l, "y");
			window.w = LuaUtils::GetField<I32>(l, "w");
			window.h = LuaUtils::GetField<I32>(l, "h");
		}
		lua_pop(l, 1);

		// Load imgui settings
		lua_getglobal(l, "imgui");
		if (lua_type(l, -1) == LUA_TSTRING) {
			imguiState = lua_tostring(l, -1);
		}
		lua_pop(l, 1);

		// Load actions
		auto& actions = app.GetActions();
		lua_getglobal(l, "actions");
		if (lua_type(l, -1) == LUA_TTABLE)
		{
			for (auto action : actions)
			{
				lua_getfield(l, -1, action->name);
				if (lua_type(l, -1) == LUA_TTABLE)
				{
					lua_getfield(l, -1, "shortcut");
					if (lua_isnumber(l, -1))
						action->shortcut = (Platform::Keycode)lua_tonumber(l, -1);
					lua_pop(l, 1);
				}
				lua_pop(l, 1);
			}
		}
		lua_pop(l, 1);

		return true;
	}

	bool Settings::Save()
	{
		FileSystem& fs = app.GetEngine().GetFileSystem();
		auto file = fs.OpenFile(SETTINGS_PATH, FileFlags::DEFAULT_WRITE);
		if (!file->IsValid())
		{
			Logger::Error("Failed to save the settings");
			return false;
		}

		// Version
		file->Write("version = 1\n");

		*file << "window = { x = " 
			<< window.x
			<< ", y = " << window.y
			<< ", w = " << window.w
			<< ", h = " << window.h << " }\n";

		// Imgui state
		file->Write("imgui = [[");
		file->Write(imguiState.c_str());
		file->Write("]]\n");

		// Actions
		auto& actions = app.GetActions();
		*file << "actions = {\n";
		for (auto action : actions)
		{
			*file << "\t" << action->name << " = { shortcut = "
				<< (int)action->shortcut << "},\n";
		}
		*file << "}\n";

		file->Close();
		return true;
	}
}
}