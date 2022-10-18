#include "modelPlugin.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"
#include "contentImporters\model\modelImporter.h"
#include "contentImporters\resourceImportingManager.h"
#include "editor\widgets\propertyEditor.h"

namespace VulkanTest
{
namespace Editor
{
	class ModelImportEntry : public ImportFileEntry
	{
	private:
		ModelImporter::ImportConfig settings;

	public:
		ModelImportEntry(const ImportRequest& request) :
			ImportFileEntry(request)
		{
		}

		bool Import() override
		{
			return ResourceImportingManager::Import(inputPath, outputPath, &settings);
		}

		void OnSettingGUI() override
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
			if (ImGui::CollapsingHeader("Geometry", flags))
			{
			}

			if (ImGui::CollapsingHeader("Transform", flags))
			{
				ATTRIBUTE_EDITOR(scale);
			}

			if (ImGui::CollapsingHeader("Level of details", flags))
			{
				ATTRIBUTE_EDITOR(autoLODs);
			}
		}

		bool HasSetting() const override
		{
			return true;
		}
	};

	ModelPlugin::ModelPlugin(EditorApp& app_) :
		app(app_)
	{
		ImportFileEntry::ImportFileEntryHandle handle;
		handle.Bind<&ModelPlugin::CreateEntry>();

		ImportFileEntry::RegisterEntry("obj", handle);
	}

	void ModelPlugin::OnGui(Span<class Resource*> resource)
	{

	}

	ImportFileEntry* ModelPlugin::CreateEntry(const ImportRequest& request)
	{
		return CJING_NEW(ModelImportEntry)(request);
	}
}
}