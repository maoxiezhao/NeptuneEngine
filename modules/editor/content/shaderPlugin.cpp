#include "shaderPlugin.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	ShaderPlugin::ShaderPlugin(EditorApp& app_) :
		app(app_)
	{
	}

	void ShaderPlugin::OnGui(Span<class Resource*> resource)
	{
		if (resource.length() > 1)
			return;

		Shader* shader = static_cast<Shader*>(resource[0]);
		if (ImGui::Button(ICON_FA_EXTERNAL_LINK_ALT "Open externally"))
			app.GetAssetBrowser().OpenInExternalEditor(shader->GetPath().c_str());
	}
}
}