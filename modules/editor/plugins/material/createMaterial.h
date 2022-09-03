#pragma once

#include "editorPlugin.h"
#include "editor\plugins\createResource.h"
#include "renderer\materials\material.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	class CreateMaterial
	{
	public:
		struct Options
		{
			MaterialInfo info;
			Color4 baseColor;
			Path textures[Texture::TextureType::COUNT];
		};

		// Creat a new importing material resource
		static bool Create(EditorApp& editor, const Guid& guid, const Path& path, const Options& options);

	private:
		static CreateResourceContext::CreateResult CreateImpl(CreateResourceContext& ctx);
	};
}
}
