#include "assetImporter.h"
#include "content\importFileEntry.h"
#include "editor\editor.h"
#include "core\platform\platform.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
    class AssetImporterImpl;

    class ImportFilesDialog final : public EditorWidget
    {
    private:
        AssetImporterImpl& impl;
        Array<ImportFileEntryPtr> importQueue;
        I32 dialogIndex;
        F32 leftColumnWidth = 160;
        I32 selectedIndex = 0;

        struct ItemNode
        {
            String name;
        };
        Array<ItemNode> items;

    public:
        ImportFilesDialog(AssetImporterImpl& impl_, I32 index_, const Array<ImportFileEntryPtr>& importQueue_) :
            impl(impl_),
            dialogIndex(index_),
            importQueue(importQueue_.copy())
        {
            for (auto entry : importQueue_)
            {
                if (entry == nullptr)
                    continue;

                auto& item = items.emplace();
                item.name = Path::GetBaseNameWithExtension(entry->inputPath);
            }
        }
        
        void OnGUI()override;
        void OnSettingsEditor();

        virtual const char* GetName() override {
            return "ImportFilesDialog";
        }
    };

    class ImporterTask final : public Thread
    {
    public:
        ImporterTask(AssetImporterImpl& impl_) :
            impl(impl_)
        {}

        I32 Task() override;

    public:
        AssetImporterImpl& impl;
        bool wasLastTickWorking = false;
        bool isFinished = false;
    };

	class AssetImporterImpl : public AssetImporter
	{
	public:
		EditorApp& editor;
		HashMap<U32, ResourceType> registeredExts;
        HashMap<U64, IPlugin*> plugins;
        Array<ImportFilesDialog*> importFileDialogs;
        Array<ImportFilesDialog*> toRemovedimportFileDialogs;

        Mutex requestMutex;
        Array<ImportRequest> importRequests;
        ImporterTask importerTask;
        Mutex queueMutex;
        Array<ImportFileEntryPtr> toImportQueue;
        Semaphore semaphore;

        volatile I32 importBatchSize = 0;
        volatile I32 importBatchDone = 0;

	public:
		AssetImporterImpl(EditorApp& editor_) :
			editor(editor_),
            semaphore(0, 0xFFff),
            importerTask(*this)
		{
            importerTask.Create("ImporterTask");
		}

		~AssetImporterImpl()
		{
            for (auto dialog : importFileDialogs) {
                CJING_DELETE(dialog);
            }
            importFileDialogs.clear();

            importerTask.isFinished = true;
            semaphore.Signal();
            importerTask.Destroy();
		}

        bool ImportImpl(const Path& input, const Path& output, bool skipDialog, bool isBuiltIn)
        {
            ScopedMutex lock(requestMutex);
            auto& job = importRequests.emplace();
            job.inputPath = input;
            job.outputPath = output;
            job.isBuilt = isBuiltIn;
            job.skipSettingsDialog = skipDialog;
            return true;
        }

        bool Import(const Path& input, const Path& output, bool skipDialog) override
        {
            if (input.IsEmpty() || output.IsEmpty())
                return false;

            String extension = Path::GetExtension(input.ToSpan());
            bool canImport = ImportFileEntry::CanImport(extension);
            if (canImport == false)
            {
                Logger::Error("Unknown resouce type:%s", input.c_str());
                return false;
            }

            // TODO: Get the output extension
            String outputExt = RESOURCE_FILES_EXTENSION_WITH_DOT;

            String basename = Path::GetBaseName(input);
            Path fullOutput = output / basename + outputExt;
            return ImportImpl(input, fullOutput, skipDialog, true);
        }

        void ShowImportFileDialog(const Path& targetLocation) override
        {
            Array<Path> files;
            auto mainWindow = editor.GetMainWindow();
            if (!Platform::OpenFileDialog(mainWindow, nullptr, "All files (*.*)\0*.*\0", true, "Select files to import", files))
                return;
            
            for (const auto& file : files)
                Import(file, targetLocation, false);
        }

        void OnGUI() override
        {
            // Show import file dialogs
            for (auto dialog : importFileDialogs)
            {
                dialog->OnGUI();

                if (!dialog->IsOpen())
                    toRemovedimportFileDialogs.push_back(dialog);
            }
            for (auto toRemoved : toRemovedimportFileDialogs)
            {
                importFileDialogs.erase(toRemoved);
                CJING_DELETE(toRemoved);
            } 
            toRemovedimportFileDialogs.clear();

            // Show importing progress bar
            I32 batchSize = AtomicRead(&importBatchSize);
            if (batchSize > 0)
            {
                const F32 uiWidth = std::max(300.f, ImGui::GetIO().DisplaySize.x * 0.33f);
                const ImVec2 pos = ImGui::GetMainViewport()->Pos;
                ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x - uiWidth) * 0.5f + pos.x, ImGui::GetIO().DisplaySize.y * 0.4f + pos.y));
                ImGui::SetNextWindowSize(ImVec2(uiWidth, -1));
                ImGui::SetNextWindowSizeConstraints(ImVec2(-FLT_MAX, 0), ImVec2(FLT_MAX, 200));
                ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                    | ImGuiWindowFlags_NoFocusOnAppearing
                    | ImGuiWindowFlags_NoInputs
                    | ImGuiWindowFlags_NoNav
                    | ImGuiWindowFlags_AlwaysAutoResize
                    | ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoSavedSettings;
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);

                if (ImGui::Begin("Resource importing", nullptr, flags))
                {
                    ImGui::Text("%s", "Importing resources...");
                    const I32 batchDone = AtomicRead(&importBatchDone);
                    ImGui::ProgressBar(((F32)batchDone) / batchSize);
                }
                ImGui::End();
                ImGui::PopStyleVar();
            }
        }

        void Update(F32 dt) override
        {
            ScopedMutex lock(requestMutex);
            if (!importRequests.empty())
            {
                // Get import entries from import requests
                Array<ImportFileEntryPtr> importEntries;
                importEntries.reserve(importRequests.size());
                bool needSettingsDialog = false;
                for (int i = 0; i < importRequests.size(); i++)
                {
                    auto& request = importRequests[i];
                    auto entry = ImportFileEntry::CreateEntry(request);
                    if (entry != nullptr)
                    {
                        if (!request.skipSettingsDialog)
                            needSettingsDialog = true;

                        importEntries.push_back(entry);
                    }
                }
                importRequests.clear();

                if (needSettingsDialog)
                {
                    auto index = importFileDialogs.size();
                    auto dialog = CJING_NEW(ImportFilesDialog)(*this, index, importEntries);
                    importFileDialogs.push_back(dialog);
                }
                else
                {
                    PushImportJobs(importEntries);
                }
            }
        }

        void PushImportJobs(Array<ImportFileEntryPtr>& jobs)
        {
            if (jobs.empty())
                return;

            ScopedMutex lock(queueMutex);
            for (const auto& job : jobs)
                toImportQueue.push_back(job);

            AtomicAdd(&importBatchSize, jobs.size());
            semaphore.Signal(jobs.size());
        }

        const char* GetName() override
        {
            return "AssetImporter";
        }
	};

    I32 ImporterTask::Task()
    {
        while (!isFinished)
        {
            ImportFileEntryPtr entry = nullptr;
            {
                // Get the next job to import
                ScopedMutex lock(impl.queueMutex);
                if (!impl.toImportQueue.empty())
                {
                    ImportFileEntryPtr temp = impl.toImportQueue.back();
                    impl.toImportQueue.pop_back();
                    entry = temp;
                }
            }

            bool inThisTickWork = (entry != nullptr);
            if (inThisTickWork)
            {
                if (!wasLastTickWorking)
                {
                    AtomicStore(&impl.importBatchDone, 0);
                }

                PROFILE_BLOCK("Compile asset");
                bool ret = entry->Import();
                if (ret == false) {
                    Logger::Error("Failed to import resource %s", entry->inputPath.c_str());
                }

                AtomicIncrement(&impl.importBatchDone);
            }
            else
            {
                if (wasLastTickWorking)
                {
                    // Import done
                    AtomicStore(&impl.importBatchSize, 0);
                    AtomicStore(&impl.importBatchDone, 0);
                }

                impl.semaphore.Wait();
            }

            wasLastTickWorking = inThisTickWork;
        }

        return 0;
    }

    void ImportFilesDialog::OnGUI()
    {
        if (!isOpen)
            return;

        ImGui::SetNextWindowFocus();
        ImGuiWindowFlags flags = 
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking;
        StaticString<32> name(ICON_FA_COOKIE "Import settings##%d", dialogIndex, flags);
        if (!ImGui::Begin(name.c_str(), &isOpen))
        {
            ImGui::End();
            return;
        }

        ImGui::Text("Specify options for importing files. Every file can have different settings. Select entries on the left panel to modify them.");

        if (ImGui::Button("Import"))
        {
            impl.PushImportJobs(importQueue);
            Close();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            Close();

        F32 contentW = ImGui::GetContentRegionAvail().x;
        ImVec2 leftSize(leftColumnWidth, 0);
        if (leftSize.x < 10) leftSize.x = 10;
        if (leftSize.x > contentW - 10) leftSize.x = contentW - 10;

        // Show left list
        {
            ImVec2 size(std::max(120.f, leftColumnWidth), 0);
            ImGui::BeginChild("left_col", size);
            ImGui::PushItemWidth(160);

            for (int i = 0; i < items.size(); i++)
            {
                auto& item = items[i];
                const bool isSelected = (selectedIndex == i);
                if (ImGui::Selectable(item.name.c_str(), isSelected))
                    selectedIndex = i;
            }

            ImGui::PopItemWidth();
            ImGui::EndChild();
        }

        ImGui::SameLine();
        ImGuiEx::VSplitter("vsplit1", &leftSize);
        if (leftSize.x >= 80)
            leftColumnWidth = leftSize.x;
        ImGui::SameLine();

        // Show right file column
        ImGui::BeginChild("file_col");
        OnSettingsEditor();
        ImGui::EndChild();

        ImGui::End();
    }

    void ImportFilesDialog::OnSettingsEditor()
    {
        if (selectedIndex < 0)
            return;

        auto entry = importQueue[selectedIndex];
        if (entry == nullptr || !entry->HasSetting())
            return;

        // TODO
        entry->OnSettingGUI();
    }

	UniquePtr<AssetImporter> AssetImporter::Create(EditorApp& app)
	{
		return CJING_MAKE_UNIQUE<AssetImporterImpl>(app);
	}
}
}