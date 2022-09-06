#pragma once

#include "stream.h"
#include "core\platform\file.h"
#include "core\filesystem\filesystem.h"

namespace VulkanTest
{
	struct VULKAN_TEST_API FileWriteStream final : public IOutputStream
	{
		using IOutputStream::Write;

		FileWriteStream(UniquePtr<File>&& file_);
		~FileWriteStream();

		FileWriteStream(const FileWriteStream& rhs) = delete;
		void operator=(const FileWriteStream& rhs) = delete;

		bool Write(const void* data, U64 size) override;
		void Close();
		U64  GetPos();
		void SetPos(U64 pos_);
		U64  Size();
		void Flush();

		FORCE_INLINE const File* GetFile() const {
			return file.Get();
		}

		static FileWriteStream* Open(FileSystem& fs, const char* path);

	private:
		UniquePtr<File> file;
		size_t bufferPos;
		U8 buffer[FILESTREAM_BUFFER_SIZE];
	};
}
