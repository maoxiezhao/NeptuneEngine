#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
namespace Editor
{
	class WorldEditor;

	struct EditorIcons
	{
		struct Hit
		{
			ECS::Entity entity;
			F32 t;
		};

		EditorIcons(WorldEditor& worldEditor_);
		~EditorIcons();

		Hit CastRayPick(const Ray& ray, U32 mask = ~0);

	private:
		WorldEditor& worldEditor;
	};

}
}
