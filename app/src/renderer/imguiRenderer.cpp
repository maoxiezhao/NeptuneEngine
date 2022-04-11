#include "imguiRenderer.h"
#include "renderer\renderPath3D.h"
#include "renderer\renderer.h"
#include "imgui_impl_glfw.h"

#include "imgui-docking\imgui.h"
#include "glfw\include\GLFW\glfw3.h"

namespace VulkanTest
{
namespace ImGuiRenderer
{
	void Initialize(App& app)
	{
		Logger::Info("Initializing imgui...");
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		// Setup Dear ImGui context
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
		io.BackendFlags = ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_RendererHasViewports | ImGuiBackendFlags_HasMouseCursors;
	
		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// Setup platform
#ifdef CJING3D_PLATFORM_WIN32
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetPlatform().GetWindow());
		ImGui_ImplGlfw_InitForVulkan(window, true);
#endif

		// Setup renderer backend
	}

	void Uninitialize()
	{
#ifdef CJING3D_PLATFORM_WIN32
		ImGui_ImplGlfw_Shutdown();
#endif

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