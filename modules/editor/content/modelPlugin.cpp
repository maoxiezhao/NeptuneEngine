#include "modelPlugin.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	ModelPlugin::ModelPlugin(EditorApp& app_) :
		app(app_)
	{
	}

	void ModelPlugin::OnGui(Span<class Resource*> resource)
	{
	}
}
}