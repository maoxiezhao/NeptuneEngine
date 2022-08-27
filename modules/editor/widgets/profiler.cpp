#include "profiler.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	ProfilerWidget::ProfilerWidget(EditorApp& editor_) :
		editor(editor_)
	{
	}

	ProfilerWidget::~ProfilerWidget()
	{
	}

	void ProfilerWidget::Update(F32 dt)
	{
	}

	void ProfilerWidget::OnGUI()
	{
		if (!isOpen) return;

		static char filter[64] = "";
		if (ImGui::Begin(ICON_FA_CHART_AREA "Profiler##profiler", &isOpen))
		{
			
		}
		ImGui::End();
	}

	const char* ProfilerWidget::GetName()
	{
		return "ProfilerWidget";
	}
}
}