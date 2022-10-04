#include "filesystem.h"
#include "core\platform\sync.h"
#include "core\platform\timer.h"
#include "core\profiler\profiler.h"

#include <queue>

namespace VulkanTest
{
	bool FileSystem::MoveFile(const char* from, const char* to)
	{
		return Platform::MoveFile(from, to);
	}

	bool FileSystem::CopyFile(const char* from, const char* to)
	{
		return Platform::FileCopy(from, to);
	}

	bool FileSystem::DeleteFile(const char* path)
	{
		return Platform::DeleteFile(path);
	}
		
	bool FileSystem::FileExists(const char* path)
	{
		return Platform::FileExists(path);
	}

	UniquePtr<File> FileSystem::OpenFile(const char* path, FileFlags flags)
	{
		MappedFile* file = CJING_NEW(MappedFile)(path, flags);
		return UniquePtr<File>(file);
	}

	bool FileSystem::StatFile(const char* path, FileInfo& stat)
	{
		return Platform::StatFile(path, stat);
	}

	U64 FileSystem::GetLastModTime(const char* path)
	{
		return Platform::GetLastModTime(path);
	}

	std::vector<ListEntry> FileSystem::Enumerate(const char* path, int mask)
	{
		std::vector<ListEntry> ret;
		auto* fileIt = Platform::CreateFileIterator(path);
		if (fileIt == nullptr) {
			return ret;
		}

		ListEntry entry;
		while (Platform::GetNextFile(fileIt, entry)) {
			ret.push_back(entry);
		}
		Platform::DestroyFileIterator(fileIt);
		return ret;
	}

	bool FileSystem::LoadContext(const char* path, OutputMemoryStream& mem)
	{
		auto file = OpenFile(path, FileFlags::DEFAULT_READ);
		if (file)
		{
			if (file->Size() <= 0)
			{
				Logger::Error("Failed to read %s", path);
				file->Close();
				return false;
			}

			mem.Resize(file->Size());
			if (!file->Read(mem.Data(), mem.Size()))
			{
				Logger::Error("Failed to read %s", path);
				file->Close();
				return false;
			}

			file->Close();
			return true;
		}

		return false;
	}
}