#include "editorIcons.h"
#include "worldEditor.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	EditorIcons::EditorIcons(WorldEditor& worldEditor_) :
		worldEditor(worldEditor_)
	{

	}

	EditorIcons::~EditorIcons()
	{
	}

	EditorIcons::Hit EditorIcons::CastRayPick(const Ray& ray, U32 mask)
	{
		return EditorIcons::Hit();
	}
}
}
