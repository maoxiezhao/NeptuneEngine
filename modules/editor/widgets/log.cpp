#include "log.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	LogWidget::LogWidget()
	{
	}

	LogWidget::~LogWidget()
	{
	}

	void LogWidget::Update(F32 dt)
	{
	}

	void LogWidget::OnGUI()
	{
		if (!isOpen)
			return;

		if (ImGui::Begin("Log", &isOpen))
		{
		}
		ImGui::End();
	}

	const char* LogWidget::GetName()
	{
		return "LogWidget";
	}
}
}