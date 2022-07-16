#pragma once

#include "IconsFontAwesome5.h"

namespace ImGuiEx
{
	IMGUI_API void Label(const char* label);
	IMGUI_API void VSplitter(const char* str_id, ImVec2* size);
	IMGUI_API void Rect(float w, float h, ImU32 color);
	IMGUI_API bool ToolbarButton(ImFont* font, const char* font_icon, const ImVec4& bg_color, const char* tooltip);
	IMGUI_API bool BeginToolbar(const char* str_id, ImVec2 screen_pos, ImVec2 size);
	IMGUI_API void EndToolbar();
	IMGUI_API bool IconButton(const char* icon, const char* tooltip);
} // namespace ImGuiEx
