#include "property.h"
#include "core\scene\reflection.h"
#include "editor\editor.h"
#include "editor\modules\level.h"
#include "editor\modules\sceneEditing.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	struct PropertyWidgetVisitor final : public Reflection::IPropertyMetaVisitor
	{
		PropertyWidgetVisitor(EditorApp& editor_, IScene* scene_, I32 index_, ECS::Entity entity_, ECS::EntityID compID_) :
			editor(editor_),
			scene(scene_),
			index(index_),
			entity(entity_),
			compID(compID_)
		{
		}

		struct AttributeInfo
		{
			float max = FLT_MAX;
			float min = -FLT_MAX;
			bool isColor = false;
		};

		template <typename T>
		static AttributeInfo GetAttributeInfo(const Reflection::PropertyMeta<T>& prop)
		{
			AttributeInfo info;
			for (const auto* attr : prop.attributes) {
				switch (attr->GetType()) {
				case Reflection::IAttribute::COLOR:
					info.isColor = true;
					break;
				default: break;
				}
			}
			return info;
		}

		template<typename T>
		void VisitImpl(const Reflection::PropertyMeta<T>& prop, std::function<void(const AttributeInfo& info)> func)
		{
			if (prop.IsReadonly())
				ImGuiEx::PushReadonly();

			AttributeInfo info = GetAttributeInfo(prop);
			ImGuiEx::Label(prop.name);
			ImGui::PushID(prop.name);
			func(info);
			ImGui::PopID();
			if (prop.IsReadonly())
				ImGuiEx::PopReadonly();
		}

		void Visit(const Reflection::PropertyMeta<F32>& prop) override
		{
			VisitImpl(prop, [&](const AttributeInfo& info) {
				F32 f = prop.Get(scene, entity, index);
				if (ImGui::DragFloat("##v", &f, 1, info.min, info.max))
				{
					f = std::clamp(f, info.min, info.max);
					prop.Set(scene, entity, index, f);
				}
			});
		}

		void Visit(const Reflection::PropertyMeta<F32x3>& prop) override
		{
			VisitImpl(prop, [&](const AttributeInfo& info) {
				F32x3 f = prop.Get(scene, entity, index);
				if (info.isColor)
				{
					if (ImGui::ColorEdit3("##v", f.data))
						prop.Set(scene, entity, index, f);
				}
				else
				{
					if (ImGui::DragFloat3("##v", f.data, 1, info.min, info.max))
						prop.Set(scene, entity, index, f);
				}
			});
		}

	private:
		EditorApp& editor;
		I32 index;
		ECS::Entity entity;
		ECS::EntityID compID;
		IScene* scene;
	};

	PropertyWidget::PropertyWidget(EditorApp& editor_) :
		editor(editor_)
	{
		componentFilter[0] = '\0';
	}

	PropertyWidget::~PropertyWidget()
	{
	}

	void PropertyWidget::Update(F32 dt)
	{
	}

	void PropertyWidget::OnGUI()
	{
		if (!isOpen) return;

		auto& entities = editor.GetSceneEditingModule().GetSelectedEntities();
		if (entities.empty())
		{
			if (ImGui::Begin("Inspector##inspector", &isOpen))
				ImGui::End();
			return;
		}

		if (entities.size() > 1)
		{
			if (ImGui::Begin(ICON_FA_INFO_CIRCLE "Inspector##inspector", &isOpen))
			{
				ImGuiEx::Label("ID");
				ImGui::Text("%s", "Multiple objects");
				ImGuiEx::Label("Name");
				ImGui::Text("%s", "Multi-object editing not supported.");
			}
			ImGui::End();
			return;
		}

		ECS::Entity entity = entities[0];
		if (ImGui::Begin("Inspector##inspector", &isOpen))
		{
			ShowBaseProperties(entity);

			entity.Each([&](ECS::EntityID compID) {
				ShowComponentProperties(entity, compID);
			});

			ImGui::Separator();
			const float x = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Add component").x - ImGui::GetStyle().FramePadding.x * 2) * 0.5f;
			ImGui::SetCursorPosX(x);
			if (ImGui::Button(ICON_FA_PLUS "Add component"))
				ImGui::OpenPopup("AddComponentPopup");

			if (ImGui::BeginPopup("AddComponentPopup", ImGuiWindowFlags_AlwaysAutoResize)) 
			{
				const float w = ImGui::GetStyle().ItemSpacing.x * 2;
				ImGui::SetNextItemWidth(-w);
				ImGui::SetNextItemWidth(200);
				ImGui::InputTextWithHint("##filter", "Filter", componentFilter, sizeof(componentFilter));
				ImGui::SameLine();
				if (ImGui::Button("Clear filter")) {
					componentFilter[0] = '\0';
				}

				ImGui::EndPopup();
			}
		}
		ImGui::End();
	}

	const char* PropertyWidget::GetName()
	{
		return "PropertyWidget";
	}

	void PropertyWidget::ShowBaseProperties(ECS::Entity entity)
	{
		const char* tmp = entity.GetName();

		char name[32];
		if (tmp != nullptr)
			CopyString(name, tmp);
		else
			memset(name, 0, sizeof(name));

		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputTextWithHint("##name", "Name", name, sizeof(name)))
			entity.SetName(name);

		if (!ImGui::TreeNodeEx("General", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		ECS::EntityID id = entity;
		ImGuiEx::Label("ID");
		ImGui::Text("%d", id);
		ECS::Entity parent = entity.GetParent();
		if (parent != ECS::INVALID_ENTITY)
		{
			const char* tmp = parent.GetName();
			ImGuiEx::Label("Parent");
			ImGui::Text("%s", tmp);
		}

		if (entity.Has<TransformComponent>())
		{
			TransformComponent* transformComp = entity.GetMut<TransformComponent>();
			ASSERT(transformComp != nullptr);

			auto& transform = transformComp->transform;
			ImGuiEx::Label("Position");
			if (ImGui::DragScalarN("##pos", ImGuiDataType_Float, transform.translation.data, 3, 1.f, 0, 0, "%.3f"))
				transform.isDirty = true;

			ImGuiEx::Label("Rotation");
			if (ImGui::DragScalarN("##rotation", ImGuiDataType_Float, transform.rotation.data, 4, 1.f, 0, 0, "%.3f"))
				transform.isDirty = true;

			F32 scale = transform.scale.x;
			ImGuiEx::Label("Scale");
			if (ImGui::DragFloat("##scale", &scale, 0.1f, 0, FLT_MAX))
			{
				transform.scale.y = transform.scale.x;
				transform.scale.z = transform.scale.x;
				transform.isDirty = true;
			}
		}

		ImGui::TreePop();
	}

	// TODO find a better way to do the reflection of the scenes
	ComponentType ConvertToCompType(World* world, ECS::EntityID compID)
	{
		if (compID == world->GetComponentID<LightComponent>())
			return Reflection::GetComponentType("Light");
		else if (compID == world->GetComponentID<ObjectComponent>())
			return Reflection::GetComponentType("Object");
		else if (compID == world->GetComponentID<MaterialComponent>())
			return Reflection::GetComponentType("Material");
		else if (compID == world->GetComponentID<MeshComponent>())
			return Reflection::GetComponentType("Mesh");

		return INVALID_COMPONENT_TYPE;
	}

	void PropertyWidget::ShowComponentProperties(ECS::Entity entity, ECS::EntityID compID)
	{
		auto world = editor.GetSceneEditingModule().GetEditingWorld();
		if (world == nullptr)
			return;

		auto compType = ConvertToCompType(world, compID);
		if (compType == INVALID_COMPONENT_TYPE)
			return;

		auto compMeta = Reflection::GetComponent(compType);
		if (compMeta == nullptr)
			return;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap;
		ImGui::Separator();
		const char* name = editor.GetComponentTypeName(compType);
		const char* icon = editor.GetComponentIcon(compType);
		ImGui::PushFont(editor.GetBoldFont());
		bool isOpen = ImGui::TreeNodeEx((void*)(U64)compID, flags, "%s%s", icon, name);
		ImGui::PopFont();

		// Show component context popup
		ImGuiStyle& style = ImGui::GetStyle();
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(ICON_FA_ELLIPSIS_V).x);
		if (ImGuiEx::IconButton(ICON_FA_ELLIPSIS_V, "Context menu"))
			ImGui::OpenPopup("ctx");
		if (ImGui::BeginPopup("ctx")) 
		{
			if (ImGui::Selectable("Remove component")) 
			{
				ImGui::EndPopup();
				if (isOpen) 
					ImGui::TreePop();
				return;
			}
			ImGui::EndPopup();
		}

		if (!isOpen)
			return;

		RenderScene* scene = dynamic_cast<RenderScene*>(world->GetScene("Renderer"));
		if (!scene)
			return;
	
		const auto meta = Reflection::GetComponent(compType);
		if (meta == nullptr)
		{
			ImGui::TreePop();
			return;
		}

		PropertyWidgetVisitor visitor(editor, scene , -1, entity, compID);
		meta->Visit(visitor);

		ImGui::TreePop();
	}
}
}
