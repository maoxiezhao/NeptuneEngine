#pragma once

#include "editorModule.h"

namespace VulkanTest
{
namespace Editor
{
	class ContentDatabaseModule : public EditorModule
	{
	public:
		ContentDatabaseModule(EditorApp& editor);
		~ContentDatabaseModule();
	};
}
}
