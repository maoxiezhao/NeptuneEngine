#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\utils\delegate.h"
#include "core\utils\path.h"

namespace VulkanTest
{
namespace Editor
{
	struct ImportRequest
	{
		Path inputPath;
		Path outputPath;
		bool isBuilt;
		bool skipSettingsDialog;
	};

	struct ImportFileEntry
	{
	public:
		using ImportFileEntryHandle = Delegate<ImportFileEntry*(const ImportRequest&)>;

		ImportFileEntry(const ImportRequest& request) :
			inputPath(request.inputPath),
			outputPath(request.outputPath)
		{
		}

		virtual bool Import();
		virtual void OnSettingGUI();
		virtual bool HasSetting()const;

	public:
		static HashMap<U64, ImportFileEntryHandle> ImportFileEntryTypes;

		static void Dispose();
		static void RegisterEntry(const String& extension, ImportFileEntryHandle handle);
		static bool CanImport(const String& extension);
		static SharedPtr<ImportFileEntry> CreateEntry(const ImportRequest& request);

	public:
		Path inputPath;
		Path outputPath;
	};
	using ImportFileEntryPtr = SharedPtr<ImportFileEntry>;
}
}
