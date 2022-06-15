#include "settings.h"
#include "editor.h"
#include "core\filesystem\filesystem.h"

namespace VulkanTest
{
namespace Editor
{
	static const char SETTINGS_PATH[] = "editor.ini";

	Settings::Settings(EditorApp& app_) :
		app(app_)
	{
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
		if (!LuaUtils::Execute(globalState, content, "settings", 0))
			return false;
		
		// Check the version
		bool validVersion = false;
		lua_getglobal(globalState, "version");
		if (LuaUtils::Get<int>(globalState, -1) == 1)
			validVersion = true;
		lua_pop(globalState, 1);

		if (validVersion == false)
		{
			Logger::Error("Failed to open %s", SETTINGS_PATH);
			return false;
		}

		lua_getglobal(globalState, "imgui");
		if (lua_type(globalState, -1) == LUA_TSTRING) {
			imguiState = lua_tostring(globalState, -1);
		}
		lua_pop(globalState, 1);

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

		// Imgui state
		file->Write("imgui = [[");
		file->Write(imguiState.c_str());
		file->Write("]]\n");

		file->Close();
		return true;
	}
}
}