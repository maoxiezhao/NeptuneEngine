#include "importFileEntry.h"

namespace VulkanTest
{
namespace Editor
{
	HashMap<U64, ImportFileEntry::ImportFileEntryHandle> ImportFileEntry::ImportFileEntryTypes;

	void ImportFileEntry::Dispose()
	{
		ImportFileEntryTypes.clear();
	}

	void ImportFileEntry::RegisterEntry(const String& extension, ImportFileEntryHandle handle)
	{
		char ext[5] = {};
		CopyString(Span(ext), extension);
		MakeLowercase(Span(ext), ext);
		ImportFileEntryTypes.insert(StringID(ext).GetHashValue(), handle);
	}

	bool ImportFileEntry::CanImport(const String& extension)
	{
		return true;
	}

	SharedPtr<ImportFileEntry> ImportFileEntry::CreateEntry(const ImportRequest& request)
	{
		char ext[5] = {};
		CopyString(Span(ext), Path::GetExtension(request.inputPath.ToSpan()));
		MakeLowercase(Span(ext), ext);

		if (StringLength(ext) <= 0)
		{
			// TODO support folder import entry
			return nullptr;
		}

		auto it = ImportFileEntryTypes.find(StringID(ext).GetHashValue());
		if (it.isValid())
			return CJING_SHARED(it.value().Invoke(request));

		return nullptr;
	}

	bool ImportFileEntry::Import()
	{
		return false;
	}

	void ImportFileEntry::OnSettingGUI()
	{
	}

	bool ImportFileEntry::HasSetting() const
	{
		return false;
	}
}
}