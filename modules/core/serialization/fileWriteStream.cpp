#include "fileWriteStream.h"

namespace VulkanTest
{
	FileWriteStream::FileWriteStream(UniquePtr<File>&& file_) :
		file(std::move(file_)),
		bufferPos(0)
	{
	}

	FileWriteStream::~FileWriteStream()
	{
		Close();
	}

	FileWriteStream* FileWriteStream::Open(FileSystem& fs, const char* path)
	{
		auto file = fs.OpenFile(path, FileFlags::DEFAULT_WRITE);
		if (!file)
		{
			Logger::Warning("Failed to write file %s", path);
			return nullptr;
		}
		return CJING_NEW(FileWriteStream)(std::move(file));
	}

	void FileWriteStream::Close()
	{
		if (file)
		{
			Flush();
			file->Close();
		}
	}

	U64 FileWriteStream::GetPos()
	{
		ASSERT(file);
		Flush();
		return file->Tell();
	}
	
	void FileWriteStream::SetPos(U64 pos_)
	{
		ASSERT(file);
		Flush();
		file->Seek(pos_);
	}

	U64 FileWriteStream::Size()
	{
		ASSERT(file);
		Flush();
		return file->Size();
	}

	void FileWriteStream::Flush()
	{
		if (bufferPos > 0)
		{
			size_t written;
			file->Write(buffer, bufferPos, written);
			bufferPos = 0;
		}
	}

	bool FileWriteStream::Write(const void* data, U64 size)
	{
		const U32 bufferBytesLeft = FILESTREAM_BUFFER_SIZE - bufferPos;
		if (size <= bufferBytesLeft)
		{
			memcpy(buffer + bufferPos, data, size);
			bufferPos += size;
		}
		else
		{
			size_t bytesWritten;

			// Flush already written bytes and write more it the buffer (reduce amount of flushes)
			if (bufferPos > 0)
			{
				memcpy(buffer + bufferPos, data, bufferBytesLeft);
				data = (U8*)data + bufferBytesLeft;
				size -= bufferBytesLeft;
				bufferPos = 0;
				file->Write(buffer, FILESTREAM_BUFFER_SIZE, bytesWritten);
			}

			// Write as much as can using whole buffer
			while (size >= FILESTREAM_BUFFER_SIZE)
			{
				memcpy(buffer, data, FILESTREAM_BUFFER_SIZE);
				data = (U8*)data + FILESTREAM_BUFFER_SIZE;
				size -= FILESTREAM_BUFFER_SIZE;
				file->Write(buffer, FILESTREAM_BUFFER_SIZE, bytesWritten);
			}

			// Write the rest of the buffer but without flushing its data
			if (size > 0)
			{
				memcpy(buffer, data, size);
				bufferPos = size;
			}
		}

		return true;
	}
}