#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	class VULKAN_EDITOR_API CodeEditor
	{
	public:
		CodeEditor(EditorApp& app_);
		~CodeEditor();

		void OpenFile(const char* path);

	private:
		EditorApp& app;
	};
}
}