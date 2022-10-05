#include "assetImporter.h"
#include "editor\editor.h"
#include "core\platform\platform.h"
#include "imgui-docking\imgui.h"
#include "assetImporter.h"

namespace VulkanTest
{
namespace Editor
{
	class AssetImporterImpl : public AssetImporter
	{
	private:
		EditorApp& editor;

	public:
		AssetImporterImpl(EditorApp& editor_) :
			editor(editor_)
		{

		}

		~AssetImporterImpl()
		{
		}
	};

	UniquePtr<AssetImporter> AssetImporter::Create(EditorApp& app)
	{
		return CJING_MAKE_UNIQUE<AssetImporter>(app);
	}
}
}