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
		void OnGUI();

		// Base properties
		struct Rect
		{
			int x, y;
			int w, h;
		};
		Rect window;
		I32 fontSize = 14;

		// Imgui states
		String imguiState;

		struct VULKAN_EDITOR_API IPlugin
		{
			virtual ~IPlugin() {}

			virtual const char* GetName()const = 0;
			virtual void OnGui(Settings& setting) = 0;
			virtual void Load(Settings& setting) = 0;
			virtual void Save(Settings& setting) = 0;
		};
		void AddPlugin(IPlugin* plugin);
		void RemovePlugin(IPlugin* plugin);

		void SetValue(const char* name, bool value) const;
		void SetValue(const char* name, float value) const;
		void SetValue(const char* name, int value) const;
		float GetValue(const char* name, float default_value) const;
		int GetValue(const char* name, int default_value) const;
		bool GetValue(const char* name, bool default_value) const;

		bool isOpen = false;

	private:
		EditorApp& app;
		lua_State* luaState;
		Array<IPlugin*> plugins;
	};
}
} 
