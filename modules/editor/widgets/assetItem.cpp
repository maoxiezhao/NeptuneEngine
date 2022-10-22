#include "assetItem.h"
#include "assetBrowser.h"
#include "editor\modules\thumbnails.h"
#include "imgui-docking\imgui.h"

#include "content\resources\texture.h"
#include "content\resources\shader.h"
#include "content\resources\model.h"
#include "content\resources\material.h"
#include "renderer\render2D\fontResource.h"

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
        itemType(type_)
	{
		char filename_[MAX_PATH_LENGTH];
		CopyString(filename_, Path::GetBaseName(path_.c_str()));
		ClampText(filename_, I32(AssetBrowser::ThumbnailSize * AssetBrowser::TileSize));
		filename = filename_;
	}

    GPU::Image* AssetItem::DefaultThumbnail() const
    {
        return nullptr;
    }

    FileItem::FileItem(const Path& path) :
        AssetItem(path, AssetItemType::File)
    {
    }

    ContentFolderItem::ContentFolderItem(const Path& path) :
        AssetItem(path, AssetItemType::Directory)
    {
    }

    GPU::Image* ContentFolderItem::DefaultThumbnail()const
    {
        return ThumbnailsModule::FolderIcon;
    }

    ResourceItem::ResourceItem(const Path& path, const Guid& id_, const ResourceType& type_) :
        AssetItem(path, AssetItemType::Resource),
        id(id_),
        type(type_),
        typeName(ResourceType::GetResourceTypename(type_))
    {
    }

    GPU::Image* ResourceItem::DefaultThumbnail()const
    {
        if (type == Shader::ResType)
            return ThumbnailsModule::ShaderIcon;
        else if (type == Model::ResType)
            return ThumbnailsModule::ModelIcon;
        else if (type == Material::ResType)
            return ThumbnailsModule::MaterialIcon;
        else if (type == Texture::ResType)
            return ThumbnailsModule::TextureIcon;
        else if (type == FontResource::ResType)
            return ThumbnailsModule::FontIcon;
        else
            return nullptr;
    }

    SceneItem::SceneItem(const Path& path, const Guid& id_) :
        ResourceItem(path, id_, SceneResource::ResType)
    {
        itemType = AssetItemType::Scene;
    }

    GPU::Image* SceneItem::DefaultThumbnail()const
    {
        return ThumbnailsModule::SceneIcon;
    }
}
}