#include "core\platform\file.h"
#include "core\platform\platform.h"

namespace VulkanTest
{
#ifdef CJING3D_PLATFORM_WIN32

	MappedFile::MappedFile(const char* path, FileFlags flags)
	{
		DWORD desiredAccess = 0;
		DWORD shareMode = 0;
		DWORD createFlags = 0;

		if (FLAG_ANY(flags, FileFlags::CREATE))
		{
			if (Platform::FileExists(path)) {
				createFlags = TRUNCATE_EXISTING;
			}
			else {
				createFlags = CREATE_ALWAYS;
			}
		}
		if (FLAG_ANY(flags, FileFlags::READ))
		{
			desiredAccess |= GENERIC_READ;
			shareMode = FILE_SHARE_READ;

			if (!FLAG_ANY(flags, FileFlags::CREATE))
				createFlags = OPEN_EXISTING;
		}
		if (FLAG_ANY(flags, FileFlags::WRITE))
		{
			desiredAccess |= GENERIC_WRITE;
		}

		handle = (HANDLE)::CreateFileA(path, desiredAccess, shareMode, nullptr, createFlags, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
		{
			DWORD err = ::GetLastError();
			Logger::Error("Failed to create file:\"%s\", error:%x", path, err);
		}
		else
		{
			//DWORD sizeH = 0;
			//DWORD sizeL = ::GetFileSize(handle, &sizeL);
			//size = (size_t)(sizeH) << 32ull | sizeL;
			size = ::GetFileSize((HANDLE)handle, 0);
		}
	}

	MappedFile::~MappedFile()
	{
		ASSERT(handle == INVALID_HANDLE_VALUE);
	}

	bool MappedFile::Read(void* buffer, size_t bytes)
	{
		U8* readBuffer = static_cast<U8*>(buffer);
		DWORD readed = 0;
		BOOL success = ::ReadFile(handle, readBuffer, (DWORD)bytes, (LPDWORD)&readed, nullptr);
		return success && bytes == readed;
	}

	bool MappedFile::Write(const void* buffer, size_t bytes)
	{
		size_t written = 0;
		const U8* writeBuffer = static_cast<const U8*>(buffer);
		BOOL success = ::WriteFile(handle, writeBuffer, (DWORD)bytes, (LPDWORD)&written, nullptr);
		return success && bytes == written;
	}

	bool MappedFile::Seek(size_t offset)
	{
		const LONG offsetHi = offset >> 32u;
		const LONG offsetLo = offset & 0xffffffffu;
		LONG offsetHiOut = offsetHi;
		return ::SetFilePointer(handle, offsetLo, &offsetHiOut, FILE_BEGIN) == (DWORD)offsetLo && offsetHiOut == offsetHi;
	}

	size_t MappedFile::Tell() const 
	{
		LONG offsetHi = 0;
		DWORD offsetLo = ::SetFilePointer(handle, 0, &offsetHi, FILE_CURRENT);
		return (size_t)offsetHi << 32ull | offsetLo;
	}

	size_t MappedFile::Size() const  {
		return size;
	}

	FileFlags MappedFile::GetFlags() const  {
		return flags;
	}

	bool MappedFile::IsValid() const  {
		return handle != INVALID_HANDLE_VALUE;
	}

	void MappedFile::Close()
	{
		if (handle != INVALID_HANDLE_VALUE)
		{
			::FlushFileBuffers(handle);
			::CloseHandle(handle);
			handle = INVALID_HANDLE_VALUE;
		}
	}
#endif
}
