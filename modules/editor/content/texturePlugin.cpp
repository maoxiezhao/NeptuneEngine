#include "texturePlugin.h"
#include "editor\editor.h"
#include "editor\widgets\propertyEditor.h"
#include "gpu\vulkan\typeToString.h"
#include "imgui-docking\imgui.h"
#include "content\resources\shader.h"
#include "contentImporters\texture\textureImporter.h"
#include "contentImporters\resourceImportingManager.h"

namespace VulkanTest
{
namespace Editor
{
	class TextureImportEntry : public ImportFileEntry
	{
	private:
		TextureImporter::ImportConfig settings;

	public:
		TextureImportEntry(const ImportRequest& request) :
			ImportFileEntry(request)
		{
		}

		bool Import() override
		{
			return ResourceImportingManager::Import(inputPath, outputPath, &settings);
		}

		void OnSettingGUI() override
		{
			ATTRIBUTE_EDITOR(generateMipmaps);
			ATTRIBUTE_EDITOR(compress);
		}

		bool HasSetting() const override
		{
			return true;
		}
	};

	ImportFileEntry* TexturePlugin::CreateEntry(const ImportRequest& request)
	{
		return CJING_NEW(TextureImportEntry)(request);
	}

	TexturePlugin::TexturePlugin(EditorApp& app_) :
		app(app_)
	{
		ImportFileEntry::ImportFileEntryHandle handle;
		handle.Bind<&TexturePlugin::CreateEntry>();

		ImportFileEntry::RegisterEntry("raw", handle);
		ImportFileEntry::RegisterEntry("png", handle);
		ImportFileEntry::RegisterEntry("jpg", handle);
		ImportFileEntry::RegisterEntry("tga", handle);
	}

	void TexturePlugin::OnGui(Span<class Resource*> resource)
	{
		if (resource.length() > 1)
			return;

		Shader* shader = static_cast<Shader*>(resource[0]);
		if (ImGui::Button(ICON_FA_EXTERNAL_LINK_ALT "Open externally"))
			app.GetAssetBrowser().OpenInExternalEditor(shader->GetPath().c_str());

		auto* texture = static_cast<Texture*>(resource[0]);
		const auto& imgInfo = texture->GetImageInfo();

		ImGuiEx::Label("Size");
		ImGui::Text("%dx%d", imgInfo.width, imgInfo.height);
		ImGuiEx::Label("Mips");
		ImGui::Text("%d", imgInfo.levels);
		ImGuiEx::Label("Format");
		ImGui::TextUnformatted(GPU::FormatToString(imgInfo.format));

		if (texture->GetImage())
		{
			ImVec2 textureSize(200, 200);
			if (imgInfo.width > imgInfo.height)
				textureSize.y = textureSize.x * imgInfo.height / imgInfo.width;
			else
				textureSize.x = textureSize.y * imgInfo.width / imgInfo.height;

			ImGui::Image(texture->GetImage(), textureSize);
		}
	}
}
}