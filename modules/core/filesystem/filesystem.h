#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\platform\file.h"
#include "core\platform\platform.h"
#include "core\utils\string.h"
#include "core\utils\path.h"
#include "core\utils\delegate.h"
#include "core\serialization\stream.h"

namespace VulkanTest
{
	struct VULKAN_TEST_API AsyncLoadHandle
	{
		U32 value;
		static const AsyncLoadHandle INVALID;

		explicit AsyncLoadHandle(U32 value_) : value(value_) {}
		bool IsValid()const {
			return value != 0xffFFffFF;
		}
	};
	using AsyncLoadCallback = Delegate<void(U64, const U8*, bool)>;

	enum class EnumrateMode
	{
		File = 1 << 0,
		Directory = 1 << 1,
		All = File | Directory
	};

	class VULKAN_TEST_API FileSystemBackend
	{
	public:
		virtual ~FileSystemBackend() = default;

		virtual void SetBasePath(const char* basePath_) = 0;
		virtual const char* GetBasePath()const = 0;
		virtual bool HasWork()const = 0;

		virtual bool MoveFile(const char* from, const char* to) = 0;
		virtual bool CopyFile(const char* from, const char* to) = 0;
		virtual bool DeleteFile(const char* path) = 0;
		virtual bool FileExists(const char* path) = 0;
		virtual UniquePtr<File> OpenFile(const char* path, FileFlags flags) = 0;
		virtual bool StatFile(const char* path, FileInfo& stat) = 0;
		virtual U64  GetLastModTime(const char* path) = 0;
		virtual std::vector<ListEntry> Enumerate(const char* path, int mask = (int)EnumrateMode::All) = 0;
		virtual bool LoadContext(const char* path, OutputMemoryStream& mem) = 0;

		virtual void ProcessAsync() = 0;
		virtual AsyncLoadHandle LoadFileAsync(const Path& path, const AsyncLoadCallback& cb) = 0;
		virtual void CancelAsync(AsyncLoadHandle async) = 0;
	};

	class VULKAN_TEST_API FileSystem
	{
	public:
		virtual ~FileSystem();

		static UniquePtr<FileSystem> Create(const char* basePath);

		bool MoveFile(const char* from, const char* to);
		bool CopyFile(const char* from, const char* to);
		bool DeleteFile(const char* path);
		bool FileExists(const char* path);
		UniquePtr<File>  OpenFile(const char* path, FileFlags flags);
		bool StatFile(const char* path, FileInfo& stat);
		U64  GetLastModTime(const char* path);
		std::vector<ListEntry> Enumerate(const char* path, int mask = (int)EnumrateMode::All);

		void SetBasePath(const char* basePath_);
		const char* GetBasePath()const;

		bool LoadContext(const char* path, OutputMemoryStream& mem);

	private:
		FileSystem(UniquePtr<FileSystemBackend>&& backend_);

		UniquePtr<FileSystemBackend> backend = nullptr;
	};
}