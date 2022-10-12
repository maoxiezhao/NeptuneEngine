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
		HashMap<U32, ResourceType> registeredExts;
        HashMap<U64, IPlugin*> plugins;
        Mutex mutex;

	public:
		AssetImporterImpl(EditorApp& editor_) :
			editor(editor_)
		{

		}

		~AssetImporterImpl()
		{
		}

        void AddPlugin(IPlugin& plugin, const std::vector<const char*>& exts) override
        {
            for (const auto& ext : exts)
            {
                const RuntimeHash hash(ext);
                ScopedMutex lock(mutex);
                plugins.insert(hash.GetHashValue(), &plugin);
            }
        }

        void AddPlugin(IPlugin& plugin) override
        {
            const auto& exts = plugin.GetSupportExtensions();
            AddPlugin(plugin, exts);
        }

        void RemovePlugin(IPlugin& plugin) override
        {
            ScopedMutex lock(mutex);
            bool finished = true;
            do
            {
                finished = true;
                for (auto iter = plugins.begin(); iter != plugins.end(); ++iter)
                {
                    if (iter.value() == &plugin)
                    {
                        plugins.erase(iter);
                        finished = false;
                        break;
                    }
                }
            } while (!finished);
        }

        // Get a taget plugin according to the extension of path
        IPlugin* GetPlugin(const Path& path)
        {
            Span<const char> ext = Path::GetExtension(path.ToSpan());
            const RuntimeHash hash(ext.data(), (U32)ext.length());
            ScopedMutex lock(mutex);
            auto it = plugins.find(hash.GetHashValue());
            return it.isValid() ? it.value() : nullptr;
        }

		ResourceType GetResourceType(const char* path) const override
		{
			Span<const char> ext = Path::GetExtension(Span(path, StringLength(path)));
			alignas(U32) char tmp[6] = {};
			CopyString(tmp, ext);
			U32 key = *(U32*)tmp;
			auto it = registeredExts.find(key);
			if (!it.isValid())
				return ResourceType::INVALID_TYPE;
			return it.value();
		}

		void RegisterExtension(const char* extension, ResourceType type) override
		{
			alignas(U32) char tmp[6] = {};
			CopyString(tmp, extension);
			U32 key = *(U32*)tmp;
			registeredExts.insert(key, type);
		}

        bool Import(const Path& input, const Path& outptu) override
        {
            return false;
        }

        void ShowImportFileDialog(const Path& targetLocation) override
        {
            Array<Path> files;
            auto mainWindow = editor.GetMainWindow();
            if (!Platform::OpenFileDialog(mainWindow, nullptr, "All files (*.*)\0*.*\0", true, "Select files to import", files))
                return;
            
            for (const auto& file : files)
                Import(file, targetLocation);
        }

        void OnGUI() override
        {

        }

        const char* GetName() override
        {
            return "AssetImporter";
        }
	};

	UniquePtr<AssetImporter> AssetImporter::Create(EditorApp& app)
	{
		return CJING_MAKE_UNIQUE<AssetImporterImpl>(app);
	}
}
}