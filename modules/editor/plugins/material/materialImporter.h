#pragma once

#include "editorPlugin.h"
#include "renderer\materials\material.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	class MaterialImporter
	{
	public:		
		static bool Import(EditorApp& app, const Path& path);
	};
}
}
