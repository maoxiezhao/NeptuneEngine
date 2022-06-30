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

			EditorRenderer& editorRenderer = app.GetEditorRenderer();
			U32x2 viewportSize = editorRenderer.GetViewportSize();
			if ((U32)size.x != viewportSize.x || (U32)size.y != viewportSize.y)
			{
				editorRenderer.SetViewportSize((U32)size.x, (U32)size.y);
				editorRenderer.ResizeBuffers();
			}
		
			auto& graph = editorRenderer.GetRenderGraph();
			auto& backRes = graph.GetOrCreateTexture("back");
			auto backTex = graph.TryGetPhysicalTexture(&backRes);
			if (backTex)
				ImGui::Image(graph.GetPhysicalTexture(backRes).GetImage(), size);
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void SceneView::Render()
	{
		app.GetRenderer()->Render();
	}

	const char* SceneView::GetName()
	{
		return "SceneView";
	}
}
}