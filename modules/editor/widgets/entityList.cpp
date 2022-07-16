#include "entityList.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"
#include "imgui-docking\imgui_internal.h"

namespace VulkanTest
{
namespace Editor
{
	EntityListWidget::EntityListWidget(EditorApp& editor_) :
		editor(editor_),
		worldEditor(editor_.GetWorldEditor())
	{
	}

	EntityListWidget::~EntityListWidget()
	{
	}

	void EntityListWidget::OnGUI()
	{
		if (!isOpen) return;

		static char filter[64] = "";
		if (ImGui::Begin(ICON_FA_STREAM "Hierarchy##hierarchy", &isOpen))
		{
			World* world = worldEditor.GetWorld();
			if (world == nullptr)
			{
				ImGui::End();
				return;
			}

			// Show filter
			const float w = ImGui::CalcTextSize(ICON_FA_TIMES).x + ImGui::GetStyle().ItemSpacing.x * 2;
			ImGui::SetNextItemWidth(-w);
			ImGui::InputTextWithHint("##filter", "Filter", filter, sizeof(filter));
			ImGui::SameLine();
			if (ImGuiEx::IconButton(ICON_FA_TIMES, "Clear filter"))
				filter[0] = '\0';

			// Show entities
			if (ImGui::BeginChild("entities"))
			{
				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x);

				if (filter[0] == '\0')
				{
					EntityFolder& folder = worldEditor.GetEntityFolder();
					OnFolderUI(folder);
				}
			}
			ImGui::EndChild();

		}
		ImGui::End();
	}

	void EntityListWidget::OnFolderUI(EntityFolder& folder)
	{
		const auto& root = folder.GetRootFolder();;
		ECS::EntityID child = root.firstEntity;
		while (child != ECS::INVALID_ENTITY)
		{
			if (worldEditor.GetWorld()->GetEntityParent(child) == ECS::INVALID_ENTITY)
				ShowHierarchy(child, worldEditor.GetSelectedEntities());
			child = folder.GetNextEntity(child);
		}
	}

	void EntityListWidget::ShowHierarchy(ECS::EntityID entity, const Array<ECS::EntityID>& selectedEntities)
	{
		Array<ECS::EntityID> children;
		World* world = worldEditor.GetWorld();
		world->EachChildren(entity, [&children](ECS::EntityID child) {
			children.push_back(child);
		});

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
		bool hasChild = children.size() > 0;
		if (!hasChild)
			flags = ImGuiTreeNodeFlags_Leaf;

		bool selected = selectedEntities.indexOf(entity) >= 0;
		if (selected)
			flags |= ImGuiTreeNodeFlags_Selected;

		bool nodeOpen;
		const ImVec2 cp = ImGui::GetCursorPos();
		ImGui::Dummy(ImVec2(1.f, ImGui::GetTextLineHeightWithSpacing()));
		if (ImGui::IsItemVisible()) 
		{
			ImGui::SetCursorPos(cp);
			const char* name = world->GetEntityName(entity);
			nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", name);
		}
		else 
		{
			const char* dummy = "";
			const ImGuiID id = ImGui::GetCurrentWindow()->GetID((void*)(intptr_t)entity);
			if (ImGui::TreeNodeBehaviorIsOpen(id, flags)) 
			{
				ImGui::SetCursorPos(cp);
				nodeOpen = ImGui::TreeNodeBehavior(id, flags, dummy, dummy);
			}
			else 
			{
				nodeOpen = false;
			}
		}

		if (ImGui::IsItemVisible())
		{
			if (!ImGui::BeginDragDropSource())
			{
				if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
					worldEditor.SelectEntities(Span(&entity, 1), ImGui::GetIO().KeyCtrl);
			}
		}

		if (nodeOpen)
		{
			for (const auto& child : children)
			{
				ShowHierarchy(child, selectedEntities);
			}
			ImGui::TreePop();
		}
	}

	const char* EntityListWidget::GetName()
	{
		return "EntityListWidget";
	}
}
}


