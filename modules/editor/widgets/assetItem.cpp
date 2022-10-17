#include "assetItem.h"
#include "assetBrowser.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
    static void ClampText(char* text, int width)
    {
        char* end = text + StringLength(text);
        ImVec2 size = ImGui::CalcTextSize(text);
        if (size.x <= width) return;

        do
        {
            *(end - 1) = '\0';
            *(end - 2) = '.';
            *(end - 3) = '.';
            *(end - 4) = '.';
            --end;

            size = ImGui::CalcTextSize(text);
        } while (size.x > width && end - text > 4);
    }

	AssetItem::AssetItem(const Path& path_, AssetItemType type_) :
		filepath(path_),
		type(type_)
	{
		char filename_[MAX_PATH_LENGTH];
		CopyString(filename_, Path::GetBaseName(path_.c_str()));
		ClampText(filename_, I32(AssetBrowser::ThumbnailSize * AssetBrowser::TileSize));
		filename = filename_;
	}

    FileItem::FileItem(const Path& path) :
        AssetItem(path, AssetItemType::File)
    {
    }

    ContentFolderItem::ContentFolderItem(const Path& path) :
        AssetItem(path, AssetItemType::Directory)
    {
    }

    ResourceItem::ResourceItem(const Path& path, const Guid& id_, const String& typename_) :
        AssetItem(path, AssetItemType::Resource),
        id(id_),
        typeName(typename_)
    {
    }
}
}