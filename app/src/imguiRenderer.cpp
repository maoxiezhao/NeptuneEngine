#include "imguiRenderer.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace ImGuiRenderer
{
	void Initialize()
	{
		Logger::Info("Initializing imgui...");
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
		io.BackendFlags = ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_RendererHasViewports | ImGuiBackendFlags_HasMouseCursors;
	}

	void Uninitialize()
	{
		ImGui::DestroyContext();
	}

	void Render(GPU::CommandList* cmd)
	{
		cmd->SetDefaultOpaqueState();
		cmd->SetProgram("screenVS.hlsl", "screenPS.hlsl");
		cmd->Draw(3);
	}
}
}