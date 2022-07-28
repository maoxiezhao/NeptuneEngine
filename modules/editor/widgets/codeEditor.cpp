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
		FileSystem& fs = app.GetEngine().GetFileSystem();
		StaticString<MAX_PATH_LENGTH> fullPath(fs.GetBasePath(), path);
		Platform::ShellExecuteOpen(fullPath.c_str(), nullptr);
	}
}
}