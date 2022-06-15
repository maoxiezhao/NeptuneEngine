#include "sceneView.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	SceneView::SceneView(EditorApp& app_) :
		app(app_)
	{
	}

	SceneView::~SceneView()
	{
	}

	void SceneView::Init()
	{
		WSI& wsi = app.GetWSI();
		graph.SetDevice(wsi.GetDevice());
	}

	void SceneView::Update(F32 dt)
	{
	}

	void SceneView::EndFrame()
	{
	}

	void SceneView::OnGUI()
	{
		PROFILE_FUNCTION();

		ImVec2 viewPos;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		if (ImGui::Begin("Scene View", nullptr, ImGuiWindowFlags_NoScrollWithMouse))
		{
			const ImVec2 size = ImGui::GetContentRegionAvail();
			if (size.x <= 0 || size.y <= 0)
			{
				ImGui::End();
				ImGui::PopStyleVar();
				return;
			}

			if ((I32)size.x != viewportSize.x || (I32)size.y != viewportSize.y)
				Bake((I32)size.x, (I32)size.y);

			GPU::DeviceVulkan* device = app.GetWSI().GetDevice();
			graph.SetupAttachments(*device, nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			Jobsystem::JobHandle handle;
			graph.Render(*device, handle);
			Jobsystem::Wait(&handle);

			auto& backRes = graph.GetOrCreateTexture("back");
			ImGui::Image(graph.GetPhysicalTexture(backRes).GetImage(), size);
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	const char* SceneView::GetName()
	{
		return "SceneView";
	}

	void SceneView::Bake(I32 w, I32 h)
	{
		viewportSize.x = w;
		viewportSize.y = h;

		WSI& wsi = app.GetWSI();

		//auto imageInfo = GPU::ImageCreateInfo::RenderTarget((U32)w, (U32)h, wsi.GetSwapchain().swapchainFormat);
		//renderTarget = device->CreateImage(imageInfo, nullptr);

		// Setup backbuffer resource dimension
		graph.Reset();

		ResourceDimensions dim;
		dim.width = w;
		dim.height = h;
		dim.format = wsi.GetSwapchain().swapchainFormat;
		graph.SetBackbufferDimension(dim);

		AttachmentInfo backInfo;
		backInfo.format = dim.format;
		backInfo.sizeX = (F32)dim.width;
		backInfo.sizeY = (F32)dim.height;

		// Compose
		auto& imguiPass = graph.AddRenderPass("MainPass", RenderGraphQueueFlag::Graphics);
		imguiPass.WriteColor("back", backInfo, "back");
		imguiPass.SetBuildCallback([&](GPU::CommandList& cmd) {
			cmd.SetDefaultOpaqueState();
			cmd.SetProgram("screenVS.hlsl", "screenPS.hlsl");
			cmd.Draw(3);
		});
		graph.SetBackBufferSource("back");
		graph.DisableSwapchain();
		graph.Bake();
	}
}
}