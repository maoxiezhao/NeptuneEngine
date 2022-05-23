#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\utils\string.h"

namespace VulkanTest
{
	enum class FileFlags
	{
		NONE = 0,
		READ = 1 << 0,
		WRITE = 1 << 1,
		CREATE = 1 << 2,
		MMAP = 1 << 3,

		DEFAULT_READ = READ | MMAP,
		DEFAULT_WRITE = WRITE | CREATE,
	};

	enum class PathType
	{
		File,
		Directory,
		Special
	};

	struct ListEntry
	{
		char filename[MAX_PATH_LENGTH];
		PathType type;
	};

	typedef U64 FileTimestamp;
	struct FileInfo 
	{
		FileTimestamp createdTime;
		FileTimestamp modifiedTime;
		size_t fileSize = 0;
		PathType type = PathType::File;
	};

	class File
	{
	public:
		virtual ~File() {}
		virtual bool   Read(void* buffer, size_t bytes) = 0;
		virtual bool   Write(const void* buffer, size_t bytes) = 0;
		virtual bool   Seek(size_t offset) = 0;
		virtual size_t Tell() const = 0;
		virtual size_t Size() const = 0;
		virtual FileFlags GetFlags() const = 0;
		virtual bool IsValid() const = 0;
		virtual void  Close() = 0;

		template <typename T> 
		bool Write(const T& value)
		{
			return Write(&value, sizeof(T));
		}

		bool Write(const char* str)
		{
			return Write(str, StringLength(str));
		}

		template <typename T> 
		void Read(T& value) 
		{ 
			Read(&value, sizeof(T));
		}
	};

	class MemFile : public File
	{
	private:
		void* data = nullptr;
		size_t size = 0;
		size_t pos = 0;
		FileFlags flags = FileFlags::NONE;

	public:
		MemFile(void* data_, size_t size_, FileFlags flags_) :
			data(data_),
			size(size_),
			flags(flags_)
		{
		}

		~MemFile()
		{
		}

		bool Read(void* buffer, size_t bytes)override
		{
			if (FLAG_ANY(flags, FileFlags::READ))
			{
				const size_t copySize = std::min(size - pos, bytes);
				Memory::Memcpy(buffer, (const U8*)data + pos, copySize);
				pos += copySize;
				return copySize > 0;
			}
			return false;
		}

		bool Write(const void* buffer, size_t bytes)override
		{
			if (FLAG_ANY(flags, FileFlags::WRITE))
			{
				const size_t copySize = std::min(size - pos, bytes);
				Memory::Memcpy((U8*)data + pos, buffer, copySize);
				pos += copySize;
				return copySize > 0;
			}
			return false;
		}

		bool Seek(size_t offset)override
		{
			if (offset < size)
			{
				pos = offset;
				return true;
			}
			return false;
		}

		size_t Tell() const override
		{
			return pos;
		}

		size_t Size() const override {
			return size;
		}

		FileFlags GetFlags() const override {
			return flags;
		}

		bool IsValid() const override {
			return data != nullptr;
		}

		void Close()override
		{
		}
	};

	class FilePathResolver;
	class MappedFile : public File
	{
	public:
		MappedFile() = default;
		MappedFile(const char* path, FileFlags flags);
		~MappedFile();

		bool Read(void* buffer, size_t bytes)override;
		bool Write(const void* buffer, size_t bytes)override;
		bool Seek(size_t offset)override;
		size_t Tell() const override;
		size_t Size() const override;
		FileFlags GetFlags() const override;
		bool IsValid() const override;
		void  Close() override;

	private:
		void* handle;
		size_t size = 0;
		FileFlags flags = FileFlags::NONE;
		volatile int mappedCount = 0;
	};
}