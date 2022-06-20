#pragma once

#include "imgui-docking\imgui.h"
#include "imgui-docking\imgui_internal.h"

namespace ImGuiUtils
{
	void VSplitter(const char* str_id, ImVec2* size)
	{
		ImVec2 screen_pos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(str_id, ImVec2(3, -1));
		ImVec2 item_rect_size = ImGui::GetItemRectSize();
		ImVec2 end_pos = { screen_pos.x + item_rect_size.x, screen_pos.y + item_rect_size.y };
		ImGuiWindow* win = ImGui::GetCurrentWindow();
		ImVec4* colors = ImGui::GetStyle().Colors;
		ImU32 color = ImGui::GetColorU32(ImGui::IsItemActive() || ImGui::IsItemHovered() ? colors[ImGuiCol_ButtonActive] : colors[ImGuiCol_Button]);
		win->DrawList->AddRectFilled(screen_pos, end_pos, color);
		
		if (ImGui::IsItemActive())
			size->x = ImMax(1.0f, ImGui::GetIO().MouseDelta.x + size->x);
	}


	void Rect(float w, float h, ImU32 color)
	{
		ImGuiWindow* win = ImGui::GetCurrentWindow();
		ImVec2 screen_pos = ImGui::GetCursorScreenPos();
		ImVec2 end_pos = { screen_pos.x + w, screen_pos.y + h };
		ImRect total_bb(screen_pos, end_pos);
		ImGui::ItemSize(total_bb);
		if (!ImGui::ItemAdd(total_bb, 0)) 
			return;
		win->DrawList->AddRectFilled(screen_pos, end_pos, color);
	}
}