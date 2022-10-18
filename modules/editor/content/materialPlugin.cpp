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

	void MaterialPlugin::OnGui(Span<class Resource*> resource)
	{

	}

	void MaterialPlugin::SaveMaterial(Material* material)
	{
	}

	ResourceType MaterialPlugin::GetResourceType() const {
		return Material::ResType;
	}
}
}