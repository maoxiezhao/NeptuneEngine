#include "settings.h"
#include "editor.h"
#include "core\filesystem\filesystem.h"
#include "core\scripts\luaUtils.h"
#include "core\scripts\luaType.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace LuaUtils
{
	template<>
	struct LuaTypeNormalMapping<ImVec2>
	{
		static ImVec2 Get(lua_State* l, int index)
		{
			F32x2 ret = LuaType<F32x2>::Get(l, index);
			return ImVec2(ret.x, ret.y);
		}

		static ImVec2 Opt(lua_State* l, int index, const ImVec2& defValue)
		{
			return LuaType<F32x2>::Check(l, index) ? Get(l, index) : defValue;
		}
	};
}

namespace Editor
{
	EditorStyle::EditorStyle(EditorApp& app_) :
		app(app_)
	{
	}

	EditorStyle::~EditorStyle()
	{
	}

	void EditorStyle::Load(lua_State* l)
	{
		lua_getglobal(l, "style");
		if (lua_type(l, -1) != LUA_TTABLE)
		{
			lua_pop(l, 1);
			return;
		}

		auto& style = ImGui::GetStyle();
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			const char* name = ImGui::GetStyleColorName(i);
			lua_getfield(l, -1, name);
			if (lua_type(l, -1) == LUA_TTABLE)
			{
				lua_rawgeti(l, -1, 1);
				if (lua_type(l, -1) == LUA_TNUMBER) style.Colors[i].x = (float)lua_tonumber(l, -1);
				lua_rawgeti(l, -2, 2);
				if (lua_type(l, -1) == LUA_TNUMBER) style.Colors[i].y = (float)lua_tonumber(l, -1);
				lua_rawgeti(l, -3, 3);
				if (lua_type(l, -1) == LUA_TNUMBER) style.Colors[i].z = (float)lua_tonumber(l, -1);
				lua_rawgeti(l, -4, 4);
				if (lua_type(l, -1) == LUA_TNUMBER) style.Colors[i].w = (float)lua_tonumber(l, -1);
				lua_pop(l, 4);
			}
			lua_pop(l, 1);
		};

#define LOAD_FLOAT(name) style.name = LuaUtils::OptField<F32>(l, #name, style.name);
#define LOAD_BOOL(name) style.name = LuaUtils::OptField<bool>(l, #name, style.name);
#define LOAD_VEC2(name) style.name = LuaUtils::OptField<ImVec2>(l, #name, style.name);

		LOAD_VEC2(WindowPadding);
		LOAD_FLOAT(WindowRounding);
		LOAD_FLOAT(WindowBorderSize);
		LOAD_VEC2(WindowMinSize);
		LOAD_VEC2(WindowTitleAlign);
		LOAD_FLOAT(ChildRounding);
		LOAD_FLOAT(ChildBorderSize);
		LOAD_FLOAT(PopupRounding);
		LOAD_FLOAT(PopupBorderSize);
		LOAD_VEC2(FramePadding);
		LOAD_FLOAT(FrameRounding);
		LOAD_FLOAT(FrameBorderSize);
		LOAD_VEC2(ItemSpacing);
		LOAD_VEC2(ItemInnerSpacing);
		LOAD_VEC2(TouchExtraPadding);
		LOAD_FLOAT(IndentSpacing);
		LOAD_FLOAT(ColumnsMinSpacing);
		LOAD_FLOAT(ScrollbarSize);
		LOAD_FLOAT(ScrollbarRounding);
		LOAD_FLOAT(GrabMinSize);
		LOAD_FLOAT(GrabRounding);
		LOAD_FLOAT(TabRounding);
		LOAD_FLOAT(TabBorderSize);
		LOAD_VEC2(ButtonTextAlign);
		LOAD_VEC2(SelectableTextAlign);
		LOAD_VEC2(DisplayWindowPadding);
		LOAD_VEC2(DisplaySafeAreaPadding);
		LOAD_FLOAT(MouseCursorScale);
		LOAD_BOOL(AntiAliasedLines);
		LOAD_BOOL(AntiAliasedFill);
		LOAD_FLOAT(CurveTessellationTol);
		LOAD_FLOAT(CircleTessellationMaxError);

#undef LOAD_FLOAT
#undef LOAD_BOOL
#undef LOAD_VEC2
		lua_pop(l, 1);
	}

	void EditorStyle::Save(File& file)
	{
		auto& style = ImGui::GetStyle();
		file << "style = {";

		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			file << "\t" << ImGui::GetStyleColorName(i) << " = {" << style.Colors[i].x
				<< ", " << style.Colors[i].y
				<< ", " << style.Colors[i].z
				<< ", " << style.Colors[i].w << "},\n";
		}

#define SAVE_FLOAT(name) do { file << "\t" #name " = " << style.name << ",\n"; } while(false)
#define SAVE_BOOL(name) do { file << "\t" #name " = " << (style.name ? "true" : "false") << ",\n"; } while(false)
#define SAVE_VEC2(name) do { file << "\t" #name " = {" << style.name.x << ", " << style.name.y << "},\n"; } while(false)

		SAVE_VEC2(WindowPadding);
		SAVE_FLOAT(WindowRounding);
		SAVE_FLOAT(WindowBorderSize);
		SAVE_VEC2(WindowMinSize);
		SAVE_VEC2(WindowTitleAlign);
		SAVE_FLOAT(ChildRounding);
		SAVE_FLOAT(ChildBorderSize);
		SAVE_FLOAT(PopupRounding);
		SAVE_FLOAT(PopupBorderSize);
		SAVE_VEC2(FramePadding);
		SAVE_FLOAT(FrameRounding);
		SAVE_FLOAT(FrameBorderSize);
		SAVE_VEC2(ItemSpacing);
		SAVE_VEC2(ItemInnerSpacing);
		SAVE_VEC2(TouchExtraPadding);
		SAVE_FLOAT(IndentSpacing);
		SAVE_FLOAT(ColumnsMinSpacing);
		SAVE_FLOAT(ScrollbarSize);
		SAVE_FLOAT(ScrollbarRounding);
		SAVE_FLOAT(GrabMinSize);
		SAVE_FLOAT(GrabRounding);
		SAVE_FLOAT(TabRounding);
		SAVE_FLOAT(TabBorderSize);
		SAVE_VEC2(ButtonTextAlign);
		SAVE_VEC2(SelectableTextAlign);
		SAVE_VEC2(DisplayWindowPadding);
		SAVE_VEC2(DisplaySafeAreaPadding);
		SAVE_FLOAT(MouseCursorScale);
		SAVE_BOOL(AntiAliasedLines);
		SAVE_BOOL(AntiAliasedFill);
		SAVE_FLOAT(CurveTessellationTol);
		SAVE_FLOAT(CircleTessellationMaxError);

#undef SAVE_BOOL
#undef SAVE_FLOAT
#undef SAVE_VEC2

		file << "}\n";
	}

	void EditorStyle::OnGUI()
	{
		ShowStyleEditor();
	}

	static void HelpMarker(const char* desc)
	{
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	bool ShowStyleSelector(const char* label)
	{
		static int style_idx = -1;
		if (ImGui::Combo(label, &style_idx, "Classic\0Dark\0Light\0"))
		{
			switch (style_idx)
			{
			case 0: ImGui::StyleColorsClassic(); break;
			case 1: ImGui::StyleColorsDark(); break;
			case 2: ImGui::StyleColorsLight(); break;
			}
			return true;
		}
		return false;
	}

	void EditorStyle::ShowStyleEditor()
	{
#ifdef _WIN32
#define IM_NEWLINE  "\r\n"
#else
#define IM_NEWLINE  "\n"
#endif

		ImGuiStyle* ref = &ImGui::GetStyle();
		// You can pass in a reference ImGuiStyle structure to compare to, revert to and save to
		// (without a reference style pointer, we will use one compared locally as a reference)
		ImGuiStyle& style = ImGui::GetStyle();
		static ImGuiStyle ref_saved_style;

		ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.50f);

		if (ShowStyleSelector("Colors##Selector"))
			ref_saved_style = style;

		// Simplified Settings (expose floating-pointer border sizes as boolean representing 0.0f or 1.0f)
		if (ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f"))
			style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
		{ bool border = (style.WindowBorderSize > 0.0f); if (ImGui::Checkbox("WindowBorder", &border)) { style.WindowBorderSize = border ? 1.0f : 0.0f; } }
		ImGui::SameLine();
		{ bool border = (style.FrameBorderSize > 0.0f);  if (ImGui::Checkbox("FrameBorder", &border)) { style.FrameBorderSize = border ? 1.0f : 0.0f; } }
		ImGui::SameLine();
		{ bool border = (style.PopupBorderSize > 0.0f);  if (ImGui::Checkbox("PopupBorder", &border)) { style.PopupBorderSize = border ? 1.0f : 0.0f; } }

		// Save/Revert button
		if (ImGui::Button("Save Ref"))
			*ref = ref_saved_style = style;
		ImGui::SameLine();
		if (ImGui::Button("Revert Ref"))
			style = *ref;
		ImGui::SameLine();
		HelpMarker(
			"Save/Revert in local non-persistent storage. Default Colors definition are not affected. "
			"Use \"Export\" below to save them somewhere.");

		ImGui::Separator();

		if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
		{
			if (ImGui::BeginTabItem("Sizes"))
			{
				ImGui::Text("Main");
				ImGui::SliderFloat2("WindowPadding", (float*)&style.WindowPadding, 0.0f, 20.0f, "%.0f");
				ImGui::SliderFloat2("FramePadding", (float*)&style.FramePadding, 0.0f, 20.0f, "%.0f");
				ImGui::SliderFloat2("ItemSpacing", (float*)&style.ItemSpacing, 0.0f, 20.0f, "%.0f");
				ImGui::SliderFloat2("ItemInnerSpacing", (float*)&style.ItemInnerSpacing, 0.0f, 20.0f, "%.0f");
				ImGui::SliderFloat2("TouchExtraPadding", (float*)&style.TouchExtraPadding, 0.0f, 10.0f, "%.0f");
				ImGui::SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 30.0f, "%.0f");
				ImGui::SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 20.0f, "%.0f");
				ImGui::SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 20.0f, "%.0f");
				ImGui::Text("Borders");
				ImGui::SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f, 1.0f, "%.0f");
				ImGui::SliderFloat("ChildBorderSize", &style.ChildBorderSize, 0.0f, 1.0f, "%.0f");
				ImGui::SliderFloat("PopupBorderSize", &style.PopupBorderSize, 0.0f, 1.0f, "%.0f");
				ImGui::SliderFloat("FrameBorderSize", &style.FrameBorderSize, 0.0f, 1.0f, "%.0f");
				ImGui::SliderFloat("TabBorderSize", &style.TabBorderSize, 0.0f, 1.0f, "%.0f");
				ImGui::Text("Rounding");
				ImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 12.0f, "%.0f");
				ImGui::SliderFloat("ChildRounding", &style.ChildRounding, 0.0f, 12.0f, "%.0f");
				ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f");
				ImGui::SliderFloat("PopupRounding", &style.PopupRounding, 0.0f, 12.0f, "%.0f");
				ImGui::SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f, 12.0f, "%.0f");
				ImGui::SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 12.0f, "%.0f");
				ImGui::SliderFloat("LogSliderDeadzone", &style.LogSliderDeadzone, 0.0f, 12.0f, "%.0f");
				ImGui::SliderFloat("TabRounding", &style.TabRounding, 0.0f, 12.0f, "%.0f");
				ImGui::Text("Alignment");
				ImGui::SliderFloat2("WindowTitleAlign", (float*)&style.WindowTitleAlign, 0.0f, 1.0f, "%.2f");
				int window_menu_button_position = style.WindowMenuButtonPosition + 1;
				if (ImGui::Combo("WindowMenuButtonPosition", (int*)&window_menu_button_position, "None\0Left\0Right\0"))
					style.WindowMenuButtonPosition = window_menu_button_position - 1;
				ImGui::Combo("ColorButtonPosition", (int*)&style.ColorButtonPosition, "Left\0Right\0");
				ImGui::SliderFloat2("ButtonTextAlign", (float*)&style.ButtonTextAlign, 0.0f, 1.0f, "%.2f");
				ImGui::SameLine(); HelpMarker("Alignment applies when a button is larger than its text content.");
				ImGui::SliderFloat2("SelectableTextAlign", (float*)&style.SelectableTextAlign, 0.0f, 1.0f, "%.2f");
				ImGui::SameLine(); HelpMarker("Alignment applies when a selectable is larger than its text content.");
				ImGui::Text("Safe Area Padding");
				ImGui::SameLine(); HelpMarker("Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).");
				ImGui::SliderFloat2("DisplaySafeAreaPadding", (float*)&style.DisplaySafeAreaPadding, 0.0f, 30.0f, "%.0f");
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Colors"))
			{
				static int output_dest = 0;
				static bool output_only_modified = true;
				if (ImGui::Button("Export"))
				{
					if (output_dest == 0)
						ImGui::LogToClipboard();
					else
						ImGui::LogToTTY();
					ImGui::LogText("ImVec4* colors = ImGui::GetStyle().Colors;" IM_NEWLINE);
					for (int i = 0; i < ImGuiCol_COUNT; i++)
					{
						const ImVec4& col = style.Colors[i];
						const char* name = ImGui::GetStyleColorName(i);
						if (!output_only_modified || memcmp(&col, &ref->Colors[i], sizeof(ImVec4)) != 0)
							ImGui::LogText("colors[ImGuiCol_%s]%*s= ImVec4(%.2ff, %.2ff, %.2ff, %.2ff);" IM_NEWLINE,
								name, 23 - (int)strlen(name), "", col.x, col.y, col.z, col.w);
					}
					ImGui::LogFinish();
				}
				ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::Combo("##output_type", &output_dest, "To Clipboard\0To TTY\0");
				ImGui::SameLine(); ImGui::Checkbox("Only Modified Colors", &output_only_modified);

				static ImGuiTextFilter filter;
				filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

				static ImGuiColorEditFlags alpha_flags = 0;
				if (ImGui::RadioButton("Opaque", alpha_flags == ImGuiColorEditFlags_None)) { alpha_flags = ImGuiColorEditFlags_None; } ImGui::SameLine();
				if (ImGui::RadioButton("Alpha", alpha_flags == ImGuiColorEditFlags_AlphaPreview)) { alpha_flags = ImGuiColorEditFlags_AlphaPreview; } ImGui::SameLine();
				if (ImGui::RadioButton("Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) { alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf; } ImGui::SameLine();
				HelpMarker(
					"In the color list:\n"
					"Left-click on colored square to open color picker,\n"
					"Right-click to open edit options menu.");

				ImGui::BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
				ImGui::PushItemWidth(-160);
				for (int i = 0; i < ImGuiCol_COUNT; i++)
				{
					const char* name = ImGui::GetStyleColorName(i);
					if (!filter.PassFilter(name))
						continue;
					ImGui::PushID(i);
					ImGui::ColorEdit4("##color", (float*)&style.Colors[i], ImGuiColorEditFlags_AlphaBar | alpha_flags);
					if (memcmp(&style.Colors[i], &ref->Colors[i], sizeof(ImVec4)) != 0)
					{
						// Tips: in a real user application, you may want to merge and use an icon font into the main font,
						// so instead of "Save"/"Revert" you'd use icons!
						// Read the FAQ and docs/FONTS.md about using icon fonts. It's really easy and super convenient!
						ImGui::SameLine(0.0f, style.ItemInnerSpacing.x); if (ImGui::Button("Save")) { ref->Colors[i] = style.Colors[i]; }
						ImGui::SameLine(0.0f, style.ItemInnerSpacing.x); if (ImGui::Button("Revert")) { style.Colors[i] = ref->Colors[i]; }
					}
					ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
					ImGui::TextUnformatted(name);
					ImGui::PopID();
				}
				ImGui::PopItemWidth();
				ImGui::EndChild();

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Rendering"))
			{
				ImGui::Checkbox("Anti-aliased lines", &style.AntiAliasedLines);
				ImGui::SameLine();
				HelpMarker("When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.");

				ImGui::Checkbox("Anti-aliased lines use texture", &style.AntiAliasedLinesUseTex);
				ImGui::SameLine();
				HelpMarker("Faster lines using texture data. Require back-end to render with bilinear filtering (not point/nearest filtering).");

				ImGui::Checkbox("Anti-aliased fill", &style.AntiAliasedFill);
				ImGui::PushItemWidth(100);
				ImGui::DragFloat("Curve Tessellation Tolerance", &style.CurveTessellationTol, 0.02f, 0.10f, 10.0f, "%.2f");
				if (style.CurveTessellationTol < 0.10f) style.CurveTessellationTol = 0.10f;

				// When editing the "Circle Segment Max Error" value, draw a preview of its effect on auto-tessellated circles.
				ImGui::DragFloat("Circle Segment Max Error", &style.CircleTessellationMaxError, 0.01f, 0.10f, 10.0f, "%.2f");
				if (ImGui::IsItemActive())
				{
					ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
					ImGui::BeginTooltip();
					ImVec2 p = ImGui::GetCursorScreenPos();
					ImDrawList* draw_list = ImGui::GetWindowDrawList();
					float RAD_MIN = 10.0f, RAD_MAX = 80.0f;
					float off_x = 10.0f;
					for (int n = 0; n < 7; n++)
					{
						const float rad = RAD_MIN + (RAD_MAX - RAD_MIN) * (float)n / (7.0f - 1.0f);
						draw_list->AddCircle(ImVec2(p.x + off_x + rad, p.y + RAD_MAX), rad, ImGui::GetColorU32(ImGuiCol_Text), 0);
						off_x += 10.0f + rad * 2.0f;
					}
					ImGui::Dummy(ImVec2(off_x, RAD_MAX * 2.0f));
					ImGui::EndTooltip();
				}
				ImGui::SameLine();
				HelpMarker("When drawing circle primitives with \"num_segments == 0\" tesselation will be calculated automatically.");

				ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f, "%.2f"); // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets). But application code could have a toggle to switch between zero and non-zero.
				ImGui::PopItemWidth();

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::PopItemWidth();
	}
}
}