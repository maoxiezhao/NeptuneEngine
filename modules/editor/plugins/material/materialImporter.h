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
		// Create a compiled resource from a importing material
		static bool Import(EditorApp& app, const Path& path);
	};
}
}
