#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "renderer\renderer.h"
#include "renderer\render2D\font.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	struct EditorIcons
	{
		enum class IconType
		{
			PointLight,
			Camera,
			Count
		};

		struct Hit
		{
			ECS::Entity entity;
			F32 dist;
		};

		struct Icon
		{
			ECS::Entity entity;
			IconType type;
			F32 scale;
		};

		EditorIcons(EditorApp& editor_);
		~EditorIcons();

		void Update(F32 dt);
		void RemoveIcon(ECS::Entity entity);
		void AddIcons(ECS::Entity entity, IconType type);
		Hit  CastRayPick(const Ray& ray, U32 mask = ~0);
		void RenderIcons(GPU::CommandList& cmd, CameraComponent& camera);

		void OnEditingSceneChanged(Scene* newScene, Scene* prevScene);

	private:
		EditorApp& editor;
		ResPtr<FontResource> iconFontRes;
		Font* iconFont = nullptr;
		HashMap<ECS::Entity, Icon> icons;
		Color4 selectedColor;
		F32 timer = 0.0f;
		Mutex mutex;
	};

}
}
