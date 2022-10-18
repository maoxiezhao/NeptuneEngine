#include "fontPlugin.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"
#include "contentImporters\resourceImportingManager.h"

namespace VulkanTest
{
namespace Editor
{
	class FontImportEntry : public ImportFileEntry
	{
	public:
		FontImportEntry(const ImportRequest& request) :
			ImportFileEntry(request)
		{
		}

		bool Import() override
		{
			return ResourceImportingManager::Import(inputPath, outputPath, nullptr);
		}
	};

	FontPlugin::FontPlugin(EditorApp& app_) :
		app(app_)
	{
		ImportFileEntry::ImportFileEntryHandle handle;
		handle.Bind<&FontPlugin::CreateEntry>();

		ImportFileEntry::RegisterEntry("ttf", handle);
	}

	ImportFileEntry* FontPlugin::CreateEntry(const ImportRequest& request)
	{
		return CJING_NEW(FontImportEntry)(request);
	}
}
}