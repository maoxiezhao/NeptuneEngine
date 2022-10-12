#include "editorModule.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	EditorModule::EditorModule(EditorApp& editor_) :
		editor(editor_)
	{
	}

	EditorModule::~EditorModule()
	{
	}

	void EditorModule::Initialize()
	{
	}

	void EditorModule::InitFinished()
	{
	}

	void EditorModule::Update()
	{
	}

	void EditorModule::Unintialize()
	{
	}
}
}