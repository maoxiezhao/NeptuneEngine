#pragma once

#include "editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	class EditorModule
	{
	public:
		EditorModule(EditorApp& editor_);
		virtual ~EditorModule();

		virtual void Initialize();
		virtual void InitFinished();
		virtual void Update();
		virtual void Unintialize();

	protected:
		EditorApp& editor;
	};
}
}
