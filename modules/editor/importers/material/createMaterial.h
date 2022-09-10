#pragma once

#include "editorPlugin.h"
#include "editor\importers\resourceCreator.h"
#include "content\resources\material.h"

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
			float roughness;
			float metalness;
			Path textures[Texture::TextureType::COUNT];
		};

		// Creat a new importing material resource
		static CreateResult Create(CreateResourceContext& ctx);
	};
}
}
