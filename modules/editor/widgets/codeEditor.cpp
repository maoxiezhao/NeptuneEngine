#include "codeEditor.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	Editor::CodeEditor::CodeEditor(EditorApp& app_) :
		app(app_)
	{
	}

	Editor::CodeEditor::~CodeEditor()
	{
	}

	void CodeEditor::OpenFile(const char* path)
	{
		StaticString<MAX_PATH_LENGTH> fullPath(path);
		Platform::ShellExecuteOpen(fullPath.c_str(), nullptr);
	}
}
}