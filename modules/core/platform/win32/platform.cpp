
/////////////////////////////////////////////////////////////////////////////////////////
// PLATFORM WIN32
////////////////////////////////////////////////////////////////////////////////////////
#ifdef CJING3D_PLATFORM_WIN32

#include "platform\platform.h"
#include "core\utils\string.h"

#include <array>

#pragma warning(push)
#pragma warning(disable : 4091)
#include <ShlObj.h>
#include <commdlg.h>
#include <shellapi.h>
#pragma warning(pop)

#pragma warning(disable : 4996)

namespace VulkanTest {
namespace Platform {

	static void WCharToChar(Span<char> out, const WCHAR* in)
	{
		const WCHAR* c = in;
		char* cout = out.begin();
		const U32 size = (U32)out.length();
		while (*c && c - in < size - 1)
		{
			*cout = (char)*c;
			++cout;
			++c;
		}
		*cout = 0;
	}


	template <int N> 
	static void CharToWChar(WCHAR(&out)[N], const char* in)
	{
		const char* c = in;
		WCHAR* cout = out;
		while (*c && c - in < N - 1)
		{
			*cout = *c;
			++cout;
			++c;
		}
		*cout = 0;
	}

	template <int N>
	struct WCharString
	{
		WCharString(const char* rhs)
		{
			CharToWChar(data, rhs);
		}

		operator const WCHAR* () const
		{
			return data;
		}

		WCHAR data[N];
	};
	using WPathString = WCharString<MAX_PATH_LENGTH>;

	U64 FileTimeToU64(FILETIME ft)
	{
		ULARGE_INTEGER i;
		i.LowPart = ft.dwLowDateTime;
		i.HighPart = ft.dwHighDateTime;
		return i.QuadPart;
	}

	struct PlatformImpl
	{
		struct {
			HCURSOR mArrow;
			HCURSOR mSizeALL;
			HCURSOR mSizeNS;
			HCURSOR mSizeWE;
			HCURSOR mSizeNWSE;
			HCURSOR mTextInput;
		} mCursors;
	};
	static PlatformImpl impl;

	void Initialize()
	{
		impl.mCursors.mArrow = LoadCursor(NULL, IDC_ARROW);
		impl.mCursors.mSizeALL = LoadCursor(NULL, IDC_SIZEALL);
		impl.mCursors.mSizeNS = LoadCursor(NULL, IDC_SIZENS);
		impl.mCursors.mSizeWE = LoadCursor(NULL, IDC_SIZEWE);
		impl.mCursors.mSizeNWSE = LoadCursor(NULL, IDC_SIZENWSE);
		impl.mCursors.mTextInput = LoadCursor(NULL, IDC_IBEAM);
	}

	void LogPlatformInfo()
	{
		Logger::Info("Platform info:");
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		Logger::Info("Page size:%d", U32(sysInfo.dwPageSize));
		Logger::Info("Num processors:%d", U32(sysInfo.dwNumberOfProcessors));
	}

	/////////////////////////////////////////////////////////////////////////////////
	// platform function
	void SetLoggerConsoleFontColor(ConsoleFontColor fontColor)
	{
		int color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		switch (fontColor)
		{
		case CONSOLE_FONT_BLUE:
			color = FOREGROUND_BLUE;
			break;
		case CONSOLE_FONT_YELLOW:
			color = FOREGROUND_RED | FOREGROUND_GREEN;
			break;
		case CONSOLE_FONT_GREEN:
			color = FOREGROUND_GREEN;
			break;
		case CONSOLE_FONT_RED:
			color = FOREGROUND_RED;
			break;
		}

		HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | static_cast<WORD>(color));
	}

	bool ShellExecuteOpen(const char* path, const char* args)
	{
		const WPathString wpath(path);
		if (args != nullptr)
		{
			const WPathString wargs(args);
			const uintptr_t ret = (uintptr_t)::ShellExecute(NULL, NULL, wpath, wargs, NULL, SW_SHOW);
			return ret >= 32;
		}
		else
		{
			const uintptr_t ret = (uintptr_t)::ShellExecute(NULL, NULL, wpath, NULL, NULL, SW_SHOW);
			return ret >= 32;
		}
	}

	bool ShellExecuteOpenAndWait(const char* path, const char* args)
	{
		const WPathString wpath(path);
		SHELLEXECUTEINFO info = {};
		info.cbSize = sizeof(SHELLEXECUTEINFO);
		info.fMask = SEE_MASK_NOCLOSEPROCESS;
		info.hwnd = NULL;
		info.lpVerb = L"open";
		info.lpFile = wpath;

		if (args != nullptr)
		{
			const WPathString wargs(args);
			info.lpParameters = wargs;
		}
		else {
			info.lpParameters = NULL;
		}

		info.lpDirectory = NULL;
		info.nShow = SW_SHOWNORMAL;
		info.hInstApp = NULL;

		if (!::ShellExecuteEx(&info)) {
			return false;
		}

		::WaitForSingleObject(info.hProcess, INFINITE);
		return true;
	}

	void CallSystem(const char* cmd)
	{
		system(cmd);
	}

	I32 GetDPI()
	{
		const HDC hdc = GetDC(NULL);
		return GetDeviceCaps(hdc, LOGPIXELSX);
	}

	void CopyToClipBoard(const char* txt)
	{
		if (!::OpenClipboard(NULL)) {
			return;
		}

		size_t len = StringLength(txt) + 1;	
		// alloc global mem
		HGLOBAL memHandle = ::GlobalAlloc(GMEM_MOVEABLE, len * sizeof(char));
		if (!memHandle) {
			return;
		}
		char* mem = (char*)::GlobalLock(memHandle);
		CopyString(Span(mem, len), txt);
		::GlobalUnlock(memHandle);

		// set clipboard
		::EmptyClipboard();
		::SetClipboardData(CF_TEXT, memHandle);
		::CloseClipboard();
	}

	bool OpenExplorer(const char* path)
	{
		const WPathString wPath(path);
		const uintptr_t ret = (uintptr_t)::ShellExecute(NULL, L"explore", wPath, NULL, NULL, SW_SHOWNORMAL);
		return ret >= 32;
	}

	I32 GetCPUsCount()
	{
		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);
		return sys_info.dwNumberOfProcessors > 0 ? sys_info.dwNumberOfProcessors : 1;
	}

	void Exit()
	{
		PostQuitMessage(0);
	}

	std::string WStringToString(const std::wstring& wstr)
	{
		if (wstr.empty()) return std::string();
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string strTo(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
	}

	std::wstring StringToWString(const std::string& str)
	{
		if (str.empty()) return std::wstring();
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
		std::wstring wstrTo(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
		return wstrTo;
	}

	ThreadID GetCurrentThreadID()
	{
		return ::GetCurrentThreadId();
	}

	static thread_local unsigned threadIndex = ~0u;
	U32 GetCurrentThreadIndex() 
	{
		if (threadIndex == ~0u)
		{
			Logger::Error("Current thread dose not set thread index.");
			return 0;
		}
		return threadIndex;
	}

	void SetCurrentThreadIndex(U32 index)
	{
		threadIndex = index;
	}

	I32 GetNumPhysicalCores()
	{
		I32 numCores = 0;
		std::array<SYSTEM_LOGICAL_PROCESSOR_INFORMATION, 256> info;
		I32 size = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

		DWORD retLen = DWORD(info.size() * size);
		::GetLogicalProcessorInformation(info.data(), &retLen);
		const I32 numInfos = retLen / size;	// calculate num of infos
		for (I32 index = 0; index < numInfos; index++)
		{
			if (info[index].Relationship == RelationProcessorCore) {
				numCores++;
			}
		}
		return numCores;
	}

	U64 GetPhysicalCoreAffinityMask(I32 core)
	{
		// get core affinity mask for thread::SetAffinity
		std::array<SYSTEM_LOGICAL_PROCESSOR_INFORMATION, 256> info;
		I32 size = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

		DWORD retLen = DWORD(info.size() * size);
		::GetLogicalProcessorInformation(info.data(), &retLen);
		const I32 numInfos = retLen / size;	// calculate num of infos
		I32 numCores = 0;
		for (I32 index = 0; index < numInfos; index++)
		{
			if (info[index].Relationship == RelationProcessorCore)
			{
				if (numCores == core) {
					return info[index].ProcessorMask;
				}
				numCores++;
			}
		}
		return 0;
	}

	void YieldCPU()
	{
		::YieldProcessor();
	}

	void Sleep(F32 seconds)
	{
		::Sleep((DWORD)(seconds * 1000));
	}

	void Barrier()
	{
		::MemoryBarrier();
	}

	void SwitchToThread()
	{
		::SwitchToThread();
	}

	WindowType GetActiveWindow()
	{
		return ::GetActiveWindow();
	}

	WindowRect GetClientBounds(WindowType window)
	{
		RECT rect;
		GetClientRect(window, &rect);
		return { (I32)rect.left, (I32)rect.top, (I32)rect.right, (I32)rect.bottom };
	}

	void SetMouseCursorType(CursorType cursorType)
	{
		switch (cursorType)
		{
		case CursorType::DEFAULT: 
			SetCursor(impl.mCursors.mArrow); break;
		case CursorType::SIZE_ALL: 
			SetCursor(impl.mCursors.mSizeALL); break;
		case CursorType::SIZE_NS:
			SetCursor(impl.mCursors.mSizeNS); break;
		case CursorType::SIZE_WE: 
			SetCursor(impl.mCursors.mSizeWE); break;
		case CursorType::SIZE_NWSE: 
			SetCursor(impl.mCursors.mSizeNWSE); break;
		case CursorType::TEXT_INPUT: 
			SetCursor(impl.mCursors.mTextInput); break;
		default: 
			break;
		}
	}

	void SetMouseCursorVisible(bool isVisible)
	{
		if (isVisible) {
			while (ShowCursor(isVisible) < 0);
		}
		else {
			while (ShowCursor(isVisible) >= 0);
		}
	}

	WindowPoint ToScreen(WindowType window, I32 x, I32 y)
	{
		POINT p;
		p.x = x;
		p.y = y;
		::ClientToScreen((HWND)window, &p);
		return { p.x, p.y };
	}

	void ShowMessageBox(const char* msg)
	{
		WCHAR tmp[2048];
		CharToWChar(tmp, msg);
		MessageBox(NULL, tmp, L"Message", MB_OK);
	}

	/////////////////////////////////////////////////////////////////////////////////
	// file function
	bool FileExists(const char* path)
	{
		const WPathString wpath(path);
		DWORD dwAttrib = GetFileAttributes(wpath);
		return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool DirExists(const char* path)
	{
		const WPathString wpath(path);
		DWORD dwAttrib = GetFileAttributes(wpath);
		return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}

	bool DeleteFile(const char* path)
	{
		const WPathString wpath(path);
		return ::DeleteFile(wpath) != FALSE;
	}

	bool MoveFile(const char* from, const char* to)
	{
		const WPathString pathFrom(from);
		const WPathString pathTo(to);
		return ::MoveFileEx(pathFrom, pathTo, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) != FALSE;
	}

	bool FileCopy(const char* from, const char* to)
	{
		const WPathString pathFrom(from);
		const WPathString pathTo(to);
		BOOL retVal = ::CopyFile(pathFrom, pathTo, FALSE);
		if (retVal == FALSE)
		{
			Logger::Warning("FileCopy failed: %x", ::GetLastError());
			return false;
		}
		return true;
	}

	size_t GetFileSize(const char* path)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		const WPathString wpath(path);
		if (!::GetFileAttributesEx(wpath, GetFileExInfoStandard, &fad)) {
			return -1;
		}

		LARGE_INTEGER size;
		size.HighPart = fad.nFileSizeHigh;
		size.LowPart  = fad.nFileSizeLow;
		return (size_t)size.QuadPart;
	}

	U64 GetLastModTime(const char* file)
	{
	/*	const WPathString wpath(file);
		FILETIME ft;
		HANDLE handle = CreateFile(wpath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (handle == INVALID_HANDLE_VALUE) {
			return 0;
		}

		if (GetFileTime(handle, NULL, NULL, &ft) == FALSE) 
		{
			CloseHandle(handle);
			return 0;
		}
		CloseHandle(handle);

		ULARGE_INTEGER i;
		i.LowPart = ft.dwLowDateTime;
		i.HighPart = ft.dwHighDateTime;
		return (U64)i.QuadPart;*/

		struct stat attrib;
		if (stat(file, &attrib) != 0)
		{
			Logger::Warning("Faild to get last mod time \"%s\"", file);
			return 0;
		}

		return attrib.st_mtime;
	}

	bool CreateDir(const char* path)
	{
		// convert "/" to "\\"
		char temp[MAX_PATH_LENGTH];
		char* out = temp;
		const char* in = path;
		while (*in && out - temp < MAX_PATH_LENGTH - 1)
		{
			*out = *in == '/' ? '\\' : *in;
			++out;
			++in;
		}
		*out = '\0';

		const WPathString wpath(temp);
		return SHCreateDirectoryEx(NULL, wpath, NULL) == ERROR_SUCCESS;
	}

	void SetCurrentDir(const char* path)
	{
		WPathString tmp(path);
		::SetCurrentDirectory(tmp);
	}

	void GetCurrentDir(Span<char> path)
	{
		WCHAR tmp[MAX_PATH_LENGTH];
		::GetCurrentDirectory(MAX_PATH_LENGTH, tmp);
		return WCharToChar(path, tmp);
	}

	bool StatFile(const char* path, FileInfo& fileInfo)
	{
		struct __stat64 buf;
		if (_wstat64(StringToWString(path).c_str(), &buf) < 0)
			return false;

		if (buf.st_mode & _S_IFREG)
			fileInfo.type = PathType::File;
		else if (buf.st_mode & _S_IFDIR)
			fileInfo.type = PathType::Directory;
		else
			fileInfo.type = PathType::Special;

		fileInfo.createdTime = buf.st_ctime;
		fileInfo.modifiedTime = buf.st_mtime;
		return true;
	}

	struct FileIterator
	{
		HANDLE handle;
		WIN32_FIND_DATA ffd;
		bool isValid;
	};

	FileIterator* CreateFileIterator(const char* path, const char* ext)
	{
		char tempPath[MAX_PATH_LENGTH] = { 0 };
		CopyString(tempPath, path);
		if (ext != nullptr)
		{
			CatString(tempPath, "/*.");
			CatString(tempPath, ext);
		}
		else
		{
			CatString(tempPath, "/*");
		}

		WCharString<MAX_PATH_LENGTH> wTempPath(tempPath);
		FileIterator* it = CJING_NEW(FileIterator);
		it->handle = ::FindFirstFile(wTempPath, &it->ffd);
		it->isValid = it->handle != INVALID_HANDLE_VALUE;
		return it;
	}

	void DestroyFileIterator(FileIterator* it)
	{
		::FindClose(it->handle);
		CJING_SAFE_DELETE(it);
	}

	bool GetNextFile(FileIterator* it, ListEntry& info)
	{
		if (!it->isValid) {
			return false;
		}

		WIN32_FIND_DATA& data = it->ffd;
		WCharToChar(info.filename, data.cFileName);
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			info.type = PathType::Directory;
		else if (data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)
			info.type = PathType::File;
		else
			info.type = PathType::Special;

		it->isValid = ::FindNextFile(it->handle, &it->ffd) != FALSE;
		return true;
	}

	void DebugOutput(const char* msg)
	{
		WPathString tmp(msg);
		::OutputDebugString(tmp);
	}

	void* LibraryOpen(const char* path)
	{
		WCHAR tmp[MAX_PATH_LENGTH];
		CharToWChar(tmp, path);
		return ::LoadLibrary(tmp);
	}

	void LibraryClose(void* handle)
	{
		::FreeLibrary((HMODULE)handle);
	}

	void* LibrarySymbol(void* handle, const char* symbolName)
	{
		return ::GetProcAddress((HMODULE)handle, symbolName);
	}

	void* MemReserve(size_t size)
	{
		return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
	}

	void MemCommit(void* ptr, size_t size)
	{
		VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
	}
	
	void MemRelease(void* ptr, size_t size)
	{
		VirtualFree(ptr, 0, MEM_RELEASE);
	}
}
}
#endif