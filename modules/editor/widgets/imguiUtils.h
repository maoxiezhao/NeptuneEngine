#pragma once

#include "imgui-docking\imgui.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui-docking\imgui_internal.h"

namespace ImGuiUtils
{
	void Label(const char* label) 
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const ImVec2 lineStart = ImGui::GetCursorScreenPos();
		const ImGuiStyle& style = ImGui::GetStyle();
		float fullWidth = ImGui::GetContentRegionAvail().x;
		float itemWidth = fullWidth * 0.6f;
		ImVec2 textSize = ImGui::CalcTextSize(label);
		ImRect textRect;
		textRect.Min = ImGui::GetCursorScreenPos();
		textRect.Max = textRect.Min;
		textRect.Max.x += fullWidth - itemWidth;
		textRect.Max.y += textSize.y;

		ImGui::AlignTextToFramePadding();
		textRect.Min.y += window->DC.CurrLineTextBaseOffset;
		textRect.Max.y += window->DC.CurrLineTextBaseOffset;

		ImGui::ItemSize(textRect);
		if (ImGui::ItemAdd(textRect, window->GetID(label)))
		{
			ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), textRect.Min, textRect.Max, textRect.Max.x,
				textRect.Max.x, label, nullptr, &textSize);

			if (textRect.GetWidth() < textSize.x && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", label);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
	}

	void VSplitter(const char* str_id, ImVec2* size)
	{
		ImVec2 screen_pos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(str_id, ImVec2(3, -1));
		ImVec2 item_rect_size = ImGui::GetItemRectSize();
		ImVec2 end_pos = screen_pos + item_rect_size;
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