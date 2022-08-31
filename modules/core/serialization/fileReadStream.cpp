#include "fileReadStream.h"

namespace VulkanTest
{
	FileReadStream::FileReadStream(UniquePtr<File>&& file_) :
		file(std::move(file_)),
		bufferPos(0),
		bufferSize(0)
	{
	}

	FileReadStream::~FileReadStream()
	{
		Close();
	}

	FileReadStream* FileReadStream::Open(FileSystem& fs, const char* path)
	{
		auto file = fs.OpenFile(path, FileFlags::DEFAULT_READ);
		if (!file)
		{
			Logger::Warning("Failed to open file %s", path);
			return nullptr;
		}
		return CJING_NEW(FileReadStream)(std::move(file));
	}

	void FileReadStream::Close()
	{
		if (file)
			file->Close();
	}

	U64 FileReadStream::GetPos()const
	{
		ASSERT(file);
		return file->Tell() - bufferSize + bufferPos;
	}
	
	void FileReadStream::SetPos(U64 pos_)
	{
		ASSERT(file);
		file->Seek(pos_);
		if (!file->Read(buffer, FILESTREAM_BUFFER_SIZE, bufferSize))
			Logger::Warning("FileReadStream failed to set pos.");
		bufferPos = 0;
	}

	bool FileReadStream::Read(void* buffer_, U64 size_)
	{
		if (size_ == 0)
			return false;

		// Need to flush buffer first
		if (bufferSize == 0)
		{
			ASSERT(bufferPos == 0);
			if (!file->Read(buffer, FILESTREAM_BUFFER_SIZE, bufferSize))
			{
				Logger::Warning("FileReadStream failed to read.");
				return false;
			}
		}

		// Readed buffer has enought data for read
		const size_t leftSize = bufferSize - bufferPos;
		if (size_ <= leftSize)
		{
			memcpy(buffer_, buffer + bufferPos, size_);
			bufferPos += size_;
			return true;
		}

		// Copy remaining readed buffer
		if (bufferPos > 0)
		{
			memcpy(buffer_, buffer + bufferPos, leftSize);
			buffer_ = (U8*)buffer_ + leftSize;
			size_ -= leftSize;
			bufferPos = 0;
			if (!file->Read(buffer, FILESTREAM_BUFFER_SIZE, bufferSize))
				goto error;
		}

		while (size_ >= FILESTREAM_BUFFER_SIZE)
		{
			memcpy(buffer_, buffer, FILESTREAM_BUFFER_SIZE);
			buffer_ = (U8*)buffer_ + FILESTREAM_BUFFER_SIZE;
			size_ -= FILESTREAM_BUFFER_SIZE;
			if (!file->Read(buffer, FILESTREAM_BUFFER_SIZE, bufferSize))
				goto error;
		}

		// size_ is already less than FILESTREAM_BUFFER_SIZE
		if (size_ > 0)
		{
			memcpy(buffer_, buffer, size_);
			bufferPos = size_;
		}

		return true;

	error:
		Logger::Warning("FileReadStream failed to read.");
		return false;
	}

	const void* FileReadStream::GetBuffer() const
	{
		ASSERT(false);
		return nullptr;
	}

	U64 FileReadStream::Size() const
	{
		return file ? file->Size() : 0;
	}
}