#include "editorIcons.h"
#include "editor\editor.h"
#include "editor\modules\level.h"
#include "renderer\render2D\render2D.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	EditorIcons::EditorIcons(EditorApp& editor_) :
		editor(editor_)
	{
		iconFontRes = ResourceManager::LoadResourceInternal<FontResource>(Path("editor/fonts/fa-solid-900"));
	}

	EditorIcons::~EditorIcons()
	{
	}

	const F32x4 selectionColor = F32x4(1, 0.6f, 0, 1);
	void EditorIcons::Update(F32 dt)
	{
		if (iconFont == nullptr && iconFontRes->IsReady())
			iconFont = iconFontRes->GetFont(32);

		timer += dt;

		F32 intensity = std::sin(timer * MATH_2PI * 0.8f) * 0.5f + 0.5f; //	[0 -1]
		F32x4 glow = Lerp(Lerp(F32x4(1, 1, 1, 1), selectionColor, 0.4f), selectionColor, intensity);
		selectedColor = Color4(glow);
	}

	void EditorIcons::RemoveIcon(ECS::Entity entity)
	{
		auto it = icons.find(entity);
		if (it.isValid())
			icons.erase(it);
	}

	void EditorIcons::AddIcons(ECS::Entity entity, IconType type)
	{
		Icon icon = {};
		icon.entity = entity;
		icon.type = type;
		icon.scale = 1.0f;
		icons.insert(entity, icon);
	}

	EditorIcons::Hit EditorIcons::CastRayPick(const Ray& ray, U32 mask)
	{
		Hit ret;
		ret.entity = ECS::INVALID_ENTITY;
		ret.dist = FLT_MAX;

		for (const auto& icon : icons)
		{
			const TransformComponent* transform = icon.entity.Get<TransformComponent>();
			if (transform == nullptr)
				continue;

			VECTOR disV = Vector3LinePointDistance(
				LoadF32x3(ray.origin), 
				LoadF32x3(ray.origin) + LoadF32x3(ray.direction), 
				LoadF32x3(transform->transform.GetPosition()));
			F32 dist = VectorGetX(disV);
			if (dist > 0.01f && dist < Distance(transform->transform.GetPosition(), ray.origin) * 0.05f && dist < ret.dist)
			{
				ret.entity = icon.entity;
				ret.dist = dist;
			}
		}

		return ret;
	}

	const F32 scaling = 0.0025f;
	void EditorIcons::RenderIcons(GPU::CommandList& cmd, CameraComponent& camera)
	{
		cmd.BeginEvent("RenderIcons");

		const auto& selectedEntities = editor.GetLevelModule().GetSelectedEntities();

		camera.UpdateCamera();
		auto vp = camera.GetViewProjection();
		auto rotation = camera.GetRotationMat();

		Render2D::TextParmas params = {};
		params.customProjection = &vp;
		params.customRotation = &rotation;	// Always face camera
		params.vAligh = Render2D::TextParmas::CENTER;
		params.hAligh = Render2D::TextParmas::CENTER;

		for (const auto& icon : icons)
		{
			const TransformComponent* transform = icon.entity.Get<TransformComponent>();
			if (transform == nullptr)
				continue;

			params.color = Color4(F32x4(1.0f, 1.0f, 1.0f, 0.5f));
			params.position = transform->transform.GetPosition();
			params.scaling = scaling * Distance(transform->transform.GetPosition(), camera.eye);

			for (auto entity : selectedEntities)
			{
				if (icon.entity == entity)
				{
					params.color = selectedColor;
					break;
				}
			}

			const char* iconString = nullptr;
			switch (icon.type)
			{
			case IconType::PointLight:
				iconString = ICON_FA_LIGHTBULB;
				break;
			default:
				break;
			}

			if (iconString != nullptr)
				Render2D::DrawText(iconFont, iconString, params, cmd);
		}

		cmd.EndEvent();
	}

	void EditorIcons::OnEditingSceneChanged(Scene* newScene, Scene* prevScene)
	{
		if (prevScene)
		{
			auto world = prevScene->GetWorld();
			// TODO use delegate to replace the SetComponenetOnAdded
			//world->SetComponenetOnAdded<LightComponent>([&](ECS::Entity entity, LightComponent& model) { AddIcons(entity, IconType::PointLight); });
			//world->SetComponenetOnRemoved<LightComponent>([&](ECS::Entity entity, LightComponent& model) { RemoveIcon(entity); });
			world->EntityDestroyed().Unbind<&EditorIcons::RemoveIcon>(this);
			icons.clear();
		}

		if (newScene)
		{
			auto world = newScene->GetWorld();
			// TODO use delegate to replace the SetComponenetOnAdded
			world->SetComponenetOnAdded<LightComponent>([&](ECS::Entity entity, LightComponent& model) { AddIcons(entity, IconType::PointLight); });
			world->SetComponenetOnRemoved<LightComponent>([&](ECS::Entity entity, LightComponent& model) { RemoveIcon(entity); });
			world->EntityDestroyed().Bind<&EditorIcons::RemoveIcon>(this);
		}
	}
}
}
