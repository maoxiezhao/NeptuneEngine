#include "materialPlugin.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	MaterialPlugin::MaterialPlugin(EditorApp& app_) :
		app(app_)
	{
	}

	bool MaterialPlugin::Import(const Path& input, const Path& outptu)
	{
		return false;
	}

	void MaterialPlugin::OnGui(Span<class Resource*> resource)
	{
		if (resource.length() > 1)
			return;

		Material* material = static_cast<Material*>(resource[0]);
		
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_SAVE "Save")) 
			SaveMaterial(material);

		char buf[MAX_PATH_LENGTH];
		const Shader* shader = material->GetShader();
		CopyString(buf, shader ? shader->GetPath().c_str() : "");
		ImGuiEx::Label("Shader");
		ImGui::Text(buf);
	}

	void MaterialPlugin::SaveMaterial(Material* material)
	{
	}

	std::vector<const char*> MaterialPlugin::GetSupportExtensions()
	{
		return { "mat" };
	}

	ResourceType MaterialPlugin::GetResourceType() const {
		return Material::ResType;
	}
}
}