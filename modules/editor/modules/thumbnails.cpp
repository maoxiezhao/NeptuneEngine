#include "thumbnails.h"
#include "editor\editor.h"
#include "renderer\render2D\sprite.h"

namespace VulkanTest
{
namespace Editor
{
	enum class AssetItemState
	{
		OK,
		OUTDATED,
		DELETED,
		NOT_CREATED,
		HAS_DEFAULT,
	};

	struct ThumbnailRequest
	{
		enum class States
		{
			Created,
			Prepared,
			Rendered,
			Disposed,
		};
		States state = States::Created;
		AssetItem* item = nullptr;
		Resource* resource = nullptr;

	public:
		bool IsReady()const {
			state == States::Prepared && resource && resource->IsLoaded();
		}

		ThumbnailRequest(AssetItem* item_) :
			item(item_)
		{
		}
	};

	GPU::Image* ThumbnailsModule::FolderIcon = nullptr;
	GPU::Image* ThumbnailsModule::SceneIcon = nullptr;
	GPU::Image* ThumbnailsModule::ShaderIcon = nullptr;
	GPU::Image* ThumbnailsModule::ModelIcon = nullptr;
	GPU::Image* ThumbnailsModule::MaterialIcon = nullptr;
	GPU::Image* ThumbnailsModule::TextureIcon = nullptr;
	GPU::Image* ThumbnailsModule::FontIcon = nullptr;

	class ThumbnailsModuleImpl : public ThumbnailsModule
	{
	private:
		Mutex mutex;
		Path cacheFolder;
		Array<ThumbnailRequest> requestQueue;

	public:
		ThumbnailsModuleImpl(EditorApp& editor) :
			ThumbnailsModule(editor)
		{
			cacheFolder = Globals::ProjectCacheFolder / "thumbnails";
		}

		void Initialize() override
		{
			if (!Platform::DirExists(cacheFolder))
				Platform::MakeDir(cacheFolder);

			auto files = FileSystem::Enumerate(cacheFolder);
			for (const auto& fileInfo : files)
			{
				if (!EndsWith(fileInfo.filename, RESOURCE_FILES_EXTENSION_WITH_DOT))
					continue;
			}
		}

		void InitFinished() override
		{
			auto renderInterface = editor.GetRenderInterface();
			if (renderInterface != nullptr)
			{
				FolderIcon =(GPU::Image*)renderInterface->LoadTexture(Path("editor/icons/tile_folder"));
				SceneIcon = (GPU::Image*)renderInterface->LoadTexture(Path("editor/icons/tile_scene"));
				ShaderIcon = (GPU::Image*)renderInterface->LoadTexture(Path("editor/icons/tile_shader"));
				ModelIcon = (GPU::Image*)renderInterface->LoadTexture(Path("editor/icons/tile_model"));
				MaterialIcon = (GPU::Image*)renderInterface->LoadTexture(Path("editor/icons/tile_material"));
				TextureIcon = (GPU::Image*)renderInterface->LoadTexture(Path("editor/icons/tile_texture"));
				FontIcon = (GPU::Image*)renderInterface->LoadTexture(Path("editor/icons/tile_font"));
			}
		}

		void Update() override
		{
			ScopedMutex lock(mutex);

		}

		AssetItemState GetItemState(const AssetItem& info)
		{
			if (info.DefaultThumbnail())
				return AssetItemState::HAS_DEFAULT;
#if 0
			if (!FileSystem::FileExists(info.filepath))
				return TileState::DELETED;

			StaticString<MAX_PATH_LENGTH> path(".export/resources_tiles/", info.filePathHash.GetHashValue(), ".tile");
			if (!FileSystem::FileExists(path))
				return TileState::NOT_CREATED;

			MaxPathString compiledPath(".export/Resources/", info.filePathHash.GetHashValue(), ".res");
			const U64 lastModified = FileSystem::GetLastModTime(path);
			if (lastModified < FileSystem::GetLastModTime(info.filepath) ||
				lastModified < FileSystem::GetLastModTime(compiledPath))
				return TileState::OUTDATED;
#endif

			return AssetItemState::NOT_CREATED;
		}

		void LoadThumbnail(AssetItem& item)
		{
		}

		void RequestThumbnail(AssetItem& item)
		{
		}

		void DeleteThumbnail(AssetItem& item)
		{
		}

		void RefreshThumbnail(AssetItem& item)
		{
			auto renderInterface = editor.GetRenderInterface();
			if (renderInterface != nullptr)
			{
				AssetItemState state = GetItemState(item);
				switch (state)
				{
				case AssetItemState::OK:
					LoadThumbnail(item);
					break;
				case AssetItemState::OUTDATED:
				case AssetItemState::NOT_CREATED:
					RequestThumbnail(item);
					break;
				case AssetItemState::DELETED:
					if (item.tex != nullptr)
					{
						renderInterface->DestroyTexture(item.tex);
						item.tex = nullptr;
					}
					DeleteThumbnail(item);
					break;
				case AssetItemState::HAS_DEFAULT:
				{
					item.tex = item.DefaultThumbnail();
				}
				break;
				default:
					break;
				}
			}
		}
	};

	ThumbnailsModule::ThumbnailsModule(EditorApp& app) :
		EditorModule(app)
	{
	}
	
	ThumbnailsModule::~ThumbnailsModule()
	{
	}

	ThumbnailsModule* ThumbnailsModule::Create(EditorApp& app)
	{
		return CJING_NEW(ThumbnailsModuleImpl)(app);
	}
}
}
