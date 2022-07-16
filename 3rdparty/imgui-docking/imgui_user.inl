
#include "imgui-docking\imgui.h"
#include "imgui-docking\imgui_internal.h"

using namespace ImGui;

namespace ImGuiEx
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

	bool ToolbarButton(ImFont* font, const char* font_icon, const ImVec4& bg_color, const char* tooltip)
	{
		auto framePadding = GetStyle().FramePadding;
		PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_Text, bg_color);
		PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, GetStyle().FramePadding.y));
		PushStyleVar(ImGuiStyleVar_WindowPadding, framePadding);
		PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

		bool ret = false;
		PushFont(font);
		if (Button(font_icon)) {
			ret = true;
		}
		PopFont();
		PopStyleColor(4);
		PopStyleVar(3);
		if (IsItemHovered()) {
			BeginTooltip();
			TextUnformatted(tooltip);
			EndTooltip();
		}
		return ret;
	}

	bool BeginToolbar(const char* str_id, ImVec2 screen_pos, ImVec2 size)
	{
		bool is_global = GImGui->CurrentWindowStack.Size == 1;
		SetNextWindowPos(screen_pos);

		ImVec2 frame_padding = GetStyle().FramePadding;
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		PushStyleVar(ImGuiStyleVar_WindowPadding, frame_padding);
		PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

		ImGuiWindowFlags flags = 
			ImGuiWindowFlags_NoTitleBar | 
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar | 
			ImGuiWindowFlags_NoSavedSettings;
		if (size.x == 0) 
			size.x = GetContentRegionAvail().x;

		SetNextWindowSize(size);
		bool ret = is_global ?  Begin(str_id, nullptr, flags) : BeginChild(str_id, size, false, flags);
		PopStyleVar(3);
		return ret;
	}

	void EndToolbar()
	{
		auto frame_padding = GetStyle().FramePadding;
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		PushStyleVar(ImGuiStyleVar_WindowPadding, frame_padding);
		PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
		ImVec2 pos = GetWindowPos();
		ImVec2 size = GetWindowSize();
		if (GImGui->CurrentWindowStack.Size == 2) 
			End(); 
		else 
			EndChild();
		PopStyleVar(3);
		if (GImGui->CurrentWindowStack.Size > 1) 
			SetCursorScreenPos(pos + ImVec2(0, size.y + GetStyle().FramePadding.y * 2));
	}

	bool IconButton(const char* icon, const char* tooltip) 
	{
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		PushStyleColor(ImGuiCol_Button, GetStyle().Colors[ImGuiCol_WindowBg]);
		bool res = SmallButton(icon);
		if (IsItemHovered()) {
			SetTooltip("%s", tooltip);
		}
		PopStyleColor();
		PopStyleVar();
		return res;
	}
}