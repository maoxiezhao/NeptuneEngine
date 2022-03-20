#include "filesystem.h"

namespace VulkanTest
{
	class DefaultFileSystemBackend : public FileSystemBackend
	{
	public:
		StaticString<MAX_PATH_LENGTH> basePath;

	public:
		DefaultFileSystemBackend(const char* basePath)
		{
			SetBasePath(basePath);
		}

		virtual ~DefaultFileSystemBackend()
		{
		}

		void SetBasePath(const char* basePath_)override
		{
			Path::Normalize(basePath_, basePath.data);
			if (!EndsWith(basePath, Path::PATH_SLASH))
				basePath.append(Path::PATH_SLASH);
		}

		const char* GetBasePath()const
		{
			return basePath;
		}

		bool HasWork()const override
		{
			return false;
		}

		bool MoveFile(const char* from, const char* to)override
		{
			MaxPathString fullPathFrom(basePath, from);
			MaxPathString fullPathTo(basePath, to);
			return Platform::MoveFile(fullPathFrom, fullPathTo);
		}

		bool CopyFile(const char* from, const char* to)override
		{
			MaxPathString fullPathFrom(basePath, from);
			MaxPathString fullPathTo(basePath, to);
			return Platform::FileCopy(fullPathFrom, fullPathTo);
		}

		bool DeleteFile(const char* path)override
		{
			MaxPathString fullPath(basePath, path);
			return Platform::DeleteFile(fullPath);
		}
		
		bool FileExists(const char* path)override
		{
			MaxPathString fullPath(basePath, path);
			return Platform::FileExists(fullPath);
		}

		UniquePtr<File> OpenFile(const char* path, FileFlags flags)override
		{
			MaxPathString fullPath(basePath, path);
			MappedFile* file = CJING_NEW(MappedFile)(path, flags);
			if (!file->IsValid())
				return nullptr;

			return UniquePtr<File>(file);
		}

		bool StatFile(const char* path, FileInfo& stat)override
		{
			MaxPathString fullPath(basePath, path);
			return Platform::StatFile(fullPath, stat);
		}

		U64 GetLastModTime(const char* path)override
		{
			MaxPathString fullPath(basePath, path);
			return Platform::GetLastModTime(fullPath);
		}

		std::vector<ListEntry> Enumerate(const char* path, int mask = (int)EnumrateMode::All)override
		{
			std::vector<ListEntry> ret;
			auto* fileIt = Platform::CreateFileIterator(path);
			if (fileIt == nullptr) {
				return ret;
			}

			ListEntry entry;
			while (Platform::GetNextFile(fileIt, entry))
			{
				ret.push_back(entry);
			}
			Platform::DestroyFileIterator(fileIt);
			return ret;
		}
	};

	UniquePtr<FileSystem> FileSystem::Create(const char* basePath)
	{
		UniquePtr<DefaultFileSystemBackend> backend = CJING_MAKE_UNIQUE<DefaultFileSystemBackend>(basePath);
		return UniquePtr<FileSystem>(new FileSystem(std::move(backend)));
	}

	bool FileSystem::MoveFile(const char* from, const char* to)
	{
		return backend->MoveFile(from, to);
	}

	bool FileSystem::CopyFile(const char* from, const char* to)
	{
		return backend->CopyFile(from, to);
	}

	bool FileSystem::DeleteFile(const char* path)
	{
		return backend->DeleteFile(path);
	}

	bool FileSystem::FileExists(const char* path)
	{
		return backend->FileExists(path);
	}

	UniquePtr<File> FileSystem::OpenFile(const char* path, FileFlags flags)
	{
		return backend->OpenFile(path, flags);
	}

	bool FileSystem::StatFile(const char* path, FileInfo& stat)
	{
		return backend->StatFile(path, stat);
	}

	U64 FileSystem::GetLastModTime(const char* path)
	{
		return backend->GetLastModTime(path);
	}

	std::vector<ListEntry> FileSystem::Enumerate(const char* path, int mask)
	{
		return backend->Enumerate(path, mask);
	}

	void FileSystem::SetBasePath(const char* basePath_)
	{
		backend->SetBasePath(basePath_);
	}

	const char* FileSystem::GetBasePath() const
	{
		return backend->GetBasePath();
	}

	FileSystem::FileSystem(UniquePtr<FileSystemBackend> backend_) :
		backend(std::move(backend_))
	{
	}
}