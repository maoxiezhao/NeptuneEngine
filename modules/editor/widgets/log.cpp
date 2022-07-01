#include "log.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	LogWidgetSink::LogWidgetSink(LogWidget& logUI_) :
		logUI(logUI_)
	{
	}

	void LogWidgetSink::Log(LogLevel level, const char* msg)
	{
		logUI.PushLog(level, msg);
	}

	LogWidget::LogWidget() :
		logSink(*this)
	{
		memset(newMessageCount, 0, sizeof(newMessageCount));
		Logger::RegisterSink(logSink);
	}

	LogWidget::~LogWidget()
	{
		Logger::UnregisterSink(logSink);
	}

	void LogWidget::PushLog(LogLevel level, const char* msg)
	{
		ScopedMutex lock(mutex);
		newMessageCount[(I32)level]++;
		auto& logMsg = messages.emplace();
		logMsg.level = level;
		logMsg.text = msg;

		if (autoscroll)
			scrollTobottom = true;
	}

	void LogWidget::Update(F32 dt)
	{
	}

	void LogTypeLabel(Span<char> output, const char* label, int count)
	{
		CopyString(output, label);
		CatString(output, "(");
		int len = StringLength(output.begin());
		ToCString(count, Span(output.pBegin + len, output.pEnd));
		CatString(output, ")###");
		CatString(output, label);
	}

	void LogWidget::OnGUI()
	{
		if (!isOpen)
			return;

		if (ImGui::Begin("Log", &isOpen))
		{
			ScopedMutex lock(mutex);
			const char* labels[] = { "Dev", "Info", "Warning", "Error" };
			for (U32 i = 0; i < LengthOf(labels); ++i)
			{
				char label[40];
				LogTypeLabel(Span(label), labels[i], newMessageCount[i]);
				if (i > 0) ImGui::SameLine();
				bool b = levelFilter & (1 << i);
				if (ImGui::Checkbox(label, &b))
				{
					if (b)
						levelFilter |= 1 << i;
					else
						levelFilter &= ~(1 << i);

					newMessageCount[i] = 0;
				}
			}

			ImGui::SameLine();
			ImGui::Checkbox("Autoscroll", &autoscroll);

			if (ImGui::BeginChild("log_messages", ImVec2(0, 0), true))
			{
				for (U32 i = 0; i < messages.size(); ++i)
				{
					auto& msg = messages[i];
					if ((levelFilter & (1 << (int)msg.level)) == 0) 
						continue;

					switch (msg.level)
					{
					case LogLevel::LVL_DEV:
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
						break;
					case LogLevel::LVL_INFO:
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
						break;
					case LogLevel::LVL_WARNING:
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
						break;
					case LogLevel::LVL_ERROR:
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
						break;
					default:
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
						break;
					}

					ImGui::TextUnformatted(msg.text);
					ImGui::PopStyleColor();
				}
				if (scrollTobottom) {
					scrollTobottom = false;
					ImGui::SetScrollHereY();
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	const char* LogWidget::GetName()
	{
		return "LogWidget";
	}
}
}