#pragma once

#ifdef CJING3D_PLATFORM_WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#if !defined(NOMINMAX) && defined(_MSC_VER)
#define NOMINMAX
#endif
#endif

#include "core\common.h"
#include "file.h"

#include <string.h>

namespace VulkanTest {
namespace Platform {

#ifdef CJING3D_PLATFORM_WIN32
	using WindowType = HWND;
	static const HWND INVALID_WINDOW = NULL;
	using ThreadID = U32;
#else 
	using WindowType = int;
	static const int INVALID_WINDOW = 0;
#endif 

	struct WindowRect
	{
		I32 mLeft = 0;
		I32 mTop = 0;
		I32 mRight = 0;
		I32 mBottom = 0;
	};

	struct WindowPoint
	{
		I32 mPosX = 0;
		I32 mPosY = 0;
	};

	enum CursorType
	{
		DEFAULT,
		SIZE_ALL,
		SIZE_NS,
		SIZE_WE,
		SIZE_NWSE,
		LOAD,
		TEXT_INPUT,

		UNDEFINED
	};

	enum ConsoleFontColor
	{
		CONSOLE_FONT_WHITE,
		CONSOLE_FONT_BLUE,
		CONSOLE_FONT_YELLOW,
		CONSOLE_FONT_GREEN,
		CONSOLE_FONT_RED
	};

	void Initialize();
	void LogPlatformInfo();
	void SetLoggerConsoleFontColor(ConsoleFontColor fontColor);
	bool ShellExecuteOpen(const char* path, const char* args);
	bool ShellExecuteOpenAndWait(const char* path, const char* args);
	void CallSystem(const char* cmd);
	I32  GetDPI();
	void CopyToClipBoard(const char* txt);
	bool OpenExplorer(const char* path);
	I32  GetCPUsCount();
	void Exit();

	std::string  WStringToString(const std::wstring& wstr);
	std::wstring StringToWString(const std::string& str);

	/////////////////////////////////////////////////////////////////////////////////
	// Threads
	ThreadID GetCurrentThreadID();
	U32 GetCurrentThreadIndex();
	void SetCurrentThreadIndex(U32 index);
	I32 GetNumPhysicalCores();
	U64 GetPhysicalCoreAffinityMask(I32 core);
	void YieldCPU();
	void Sleep(F32 seconds);
	void Barrier();
	void SwitchToThread();

	/////////////////////////////////////////////////////////////////////////////////
	// Window
	WindowType GetActiveWindow();
	WindowRect GetClientBounds(WindowType window);
	void SetMouseCursorType(CursorType cursorType);
	void SetMouseCursorVisible(bool isVisible);

	struct WindowInitArgs {
		enum Flags {
			NO_DECORATION = 1 << 0,
			NO_TASKBAR_ICON = 1 << 1
		};
		const char* mName = "";
		bool mFullscreen = false;
		U32  mFlags = 0;
		void* mParent = nullptr;
	};
	WindowType CreateSimpleWindow(const WindowInitArgs& args);
	void DestroySimpleWindow(WindowType window);
	void SetSimpleWindowRect(WindowType window, const WindowRect& rect);
	WindowRect GetSimpleWindowRect(WindowType window);
	WindowPoint ToScreen(WindowType window, I32 x, I32 y);

	void ShowMessageBox(const char* msg);

	/////////////////////////////////////////////////////////////////////////////////
	// File 
	struct FileIterator;

	bool   FileExists(const char* path);
	bool   DirExists(const char* path);
	bool   DeleteFile(const char* path);
	bool   MoveFile(const char* from, const char* to);
	bool   FileCopy(const char* from, const char* to);
	size_t GetFileSize(const char* path);
	U64    GetLastModTime(const char* file);
	bool   CreateDir(const char* path);
	void   SetCurrentDir(const char* path);
	void   GetCurrentDir(Span<char> path);
	bool   StatFile(const char* path, FileInfo& fileInfo);
	FileIterator* CreateFileIterator(const char* path, const char* ext = nullptr);
	void DestroyFileIterator(FileIterator* it);
	bool GetNextFile(FileIterator* it, ListEntry& info);

	void DebugOutput(const char* msg);

	/////////////////////////////////////////////////////////////////////////////////
	// Library
	void* LibraryOpen(const char* path);
	void  LibraryClose(void* handle);
	void* LibrarySymbol(void* handle, const char* symbolName);

	/////////////////////////////////////////////////////////////////////////////////
	// Memory
	void* MemReserve(size_t size);
	void MemCommit(void* ptr, size_t size);
	void MemRelease(void* ptr, size_t size);
}
}