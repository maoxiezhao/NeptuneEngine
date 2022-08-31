#pragma once

#include "stream.h"
#include "core\platform\file.h"
#include "core\filesystem\filesystem.h"

namespace VulkanTest
{
	#define FILESTREAM_BUFFER_SIZE 4096

	struct VULKAN_TEST_API FileReadStream final : public IInputStream
	{
		using IInputStream::Read;

		FileReadStream(UniquePtr<File>&& file_);
		~FileReadStream();

		FileReadStream(const FileReadStream& rhs) = delete;
		void operator=(const FileReadStream& rhs) = delete;

		bool Read(void* buffer_, U64 size_) override;
		void Close();
		U64 GetPos()const override;
		void SetPos(U64 pos_) override;
		const void* GetBuffer() const override;
		U64 Size() const override;

		FORCE_INLINE const File* GetFile() const {
			return file.Get();
		}

		static FileReadStream* Open(FileSystem& fs, const char* path);

	private:
		UniquePtr<File> file;
		size_t bufferPos;
		size_t bufferSize;
		U8 buffer[FILESTREAM_BUFFER_SIZE];
	};
}
