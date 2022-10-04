#pragma once

#include "defines.h"
#include "core\common.h"
#include "core\utils\delegate.h"
#include "file.h"

#include <string.h>

namespace VulkanTest {
namespace Platform {

#ifdef CJING3D_PLATFORM_WIN32
	using WindowType = HWND;
	static const HWND INVALID_WINDOW = NULL;
	using ThreadID = U64;
#else 
	using WindowType = int;
	static const int INVALID_WINDOW = 0;
#endif 

	struct WindowRect
	{
		I32 left = 0;
		I32 top = 0;
		I32 width = 0;
		I32 height = 0;
	};

	struct WindowPoint
	{
		I32 x = 0;
		I32 y = 0;
	};

	struct Monitor
	{
		WindowRect workRect;
		WindowRect monitorRect;
		bool primary;
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

	struct MemoryStats
	{
		U64 totalPhysicalMemory;
		U64 usedPhysicalMemory;
		U64 totalVirtualMemory;
		U64 usedVirtualMemory;
	};

	struct ProcessMemoryStats
	{
		U64 usedPhysicalMemory;
		U64 usedVirtualMemory;
	};

	void Initialize();
	void Uninitialize();
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
	WindowPoint GetMouseScreenPos();
	void SetMouseScreenPos(int x, int y);
	void GrabMouse(WindowType window);

	enum class Keycode : U8 
	{
		LBUTTON = 0x01,
		RBUTTON = 0x02,
		CANCEL = 0x03,
		MBUTTON = 0x04,
		BACKSPACE = 0x08,
		TAB = 0x09,
		CLEAR = 0x0C,
		RETURN = 0x0D,
		SHIFT = 0x10,
		CTRL = 0x11,
		MENU = 0x12, // alt
		PAUSE = 0x13,
		CAPITAL = 0x14,
		KANA = 0x15,
		HANGEUL = 0x15,
		HANGUL = 0x15,
		JUNJA = 0x17,
		FINAL = 0x18,
		HANJA = 0x19,
		KANJI = 0x19,
		ESCAPE = 0x1B,
		CONVERT = 0x1C,
		NONCONVERT = 0x1D,
		ACCEPT = 0x1E,
		MODECHANGE = 0x1F,
		SPACE = 0x20,
		PAGEUP = 0x21,
		PAGEDOWN = 0x22,
		END = 0x23,
		HOME = 0x24,
		LEFT = 0x25,
		UP = 0x26,
		RIGHT = 0x27,
		DOWN = 0x28,
		SELECT = 0x29,
		PRINT = 0x2A,
		EXECUTE = 0x2B,
		SNAPSHOT = 0x2C,
		INSERT = 0x2D,
		DEL = 0x2E,
		HELP = 0x2F,
		LWIN = 0x5B,
		RWIN = 0x5C,
		APPS = 0x5D,
		SLEEP = 0x5F,
		NUMPAD0 = 0x60,
		NUMPAD1 = 0x61,
		NUMPAD2 = 0x62,
		NUMPAD3 = 0x63,
		NUMPAD4 = 0x64,
		NUMPAD5 = 0x65,
		NUMPAD6 = 0x66,
		NUMPAD7 = 0x67,
		NUMPAD8 = 0x68,
		NUMPAD9 = 0x69,
		MULTIPLY = 0x6A,
		ADD = 0x6B,
		SEPARATOR = 0x6C,
		SUBTRACT = 0x6D,
		DECIMAL = 0x6E,
		DIVIDE = 0x6F,
		F1 = 0x70,
		F2 = 0x71,
		F3 = 0x72,
		F4 = 0x73,
		F5 = 0x74,
		F6 = 0x75,
		F7 = 0x76,
		F8 = 0x77,
		F9 = 0x78,
		F10 = 0x79,
		F11 = 0x7A,
		F12 = 0x7B,
		F13 = 0x7C,
		F14 = 0x7D,
		F15 = 0x7E,
		F16 = 0x7F,
		F17 = 0x80,
		F18 = 0x81,
		F19 = 0x82,
		F20 = 0x83,
		F21 = 0x84,
		F22 = 0x85,
		F23 = 0x86,
		F24 = 0x87,
		NUMLOCK = 0x90,
		SCROLL = 0x91,
		OEM_NEC_EQUAL = 0x92,
		OEM_FJ_JISHO = 0x92,
		OEM_FJ_MASSHOU = 0x93,
		OEM_FJ_TOUROKU = 0x94,
		OEM_FJ_LOYA = 0x95,
		OEM_FJ_ROYA = 0x96,
		LSHIFT = 0xA0,
		RSHIFT = 0xA1,
		LCTRL = 0xA2,
		RCTRL = 0xA3,
		LMENU = 0xA4,
		RMENU = 0xA5,
		BROWSER_BACK = 0xA6,
		BROWSER_FORWARD = 0xA7,
		BROWSER_REFRESH = 0xA8,
		BROWSER_STOP = 0xA9,
		BROWSER_SEARCH = 0xAA,
		BROWSER_FAVORITES = 0xAB,
		BROWSER_HOME = 0xAC,
		VOLUME_MUTE = 0xAD,
		VOLUME_DOWN = 0xAE,
		VOLUME_UP = 0xAF,
		MEDIA_NEXT_TRACK = 0xB0,
		MEDIA_PREV_TRACK = 0xB1,
		MEDIA_STOP = 0xB2,
		MEDIA_PLAY_PAUSE = 0xB3,
		LAUNCH_MAIL = 0xB4,
		LAUNCH_MEDIA_SELECT = 0xB5,
		LAUNCH_APP1 = 0xB6,
		LAUNCH_APP2 = 0xB7,
		OEM_1 = 0xBA,
		OEM_PLUS = 0xBB,
		OEM_COMMA = 0xBC,
		OEM_MINUS = 0xBD,
		OEM_PERIOD = 0xBE,
		OEM_2 = 0xBF,
		OEM_3 = 0xC0,
		OEM_4 = 0xDB,
		OEM_5 = 0xDC,
		OEM_6 = 0xDD,
		OEM_7 = 0xDE,
		OEM_8 = 0xDF,
		OEM_AX = 0xE1,
		OEM_102 = 0xE2,
		ICO_HELP = 0xE3,
		ICO_00 = 0xE4,
		PROCESSKEY = 0xE5,
		ICO_CLEAR = 0xE6,
		PACKET = 0xE7,
		OEM_RESET = 0xE9,
		OEM_JUMP = 0xEA,
		OEM_PA1 = 0xEB,
		OEM_PA2 = 0xEC,
		OEM_PA3 = 0xED,
		OEM_WSCTRL = 0xEE,
		OEM_CUSEL = 0xEF,
		OEM_ATTN = 0xF0,
		OEM_FINISH = 0xF1,
		OEM_COPY = 0xF2,
		OEM_AUTO = 0xF3,
		OEM_ENLW = 0xF4,
		OEM_BACKTAB = 0xF5,
		ATTN = 0xF6,
		CRSEL = 0xF7,
		EXSEL = 0xF8,
		EREOF = 0xF9,
		PLAY = 0xFA,
		ZOOM = 0xFB,
		NONAME = 0xFC,
		PA1 = 0xFD,
		OEM_CLEAR = 0xFE,

		A = 'A',
		C = 'C',
		D = 'D',
		E = 'E',
		K = 'K',
		S = 'S',
		V = 'V',
		X = 'X',
		Y = 'Y',
		Z = 'Z',

		INVALID = 0,
		MAX = 0xff
	};

	enum class MouseButton : I32 
	{
		LEFT = 0,
		RIGHT = 1,
		MIDDLE = 2,
		COUNT = 3,
		MAX = 16
	};

	struct WindowEvent 
	{
		enum class Type 
		{
			QUIT,
			KEY,
			CHAR,
			MOUSE_BUTTON,
			MOUSE_MOVE,
			MOUSE_WHEEL,
			WINDOW_CLOSE,
			WINDOW_SIZE,
			WINDOW_MOVE,
			DROP_FILE,
			FOCUS
		};

		Type type;
		WindowType window;
		union {
			struct { U32 utf8; } textInput;
			struct { int w, h; } winSize;
			struct { int x, y; } winMove;
			struct { bool down; MouseButton button; } mouseButton;
			struct { int xrel, yrel; } mouseMove;
			struct { float amount; } mouseWheel;
			struct { bool down; Keycode keycode; } key;
			struct { void* handle; } fileDrop;
			struct { bool gained; } focus;
		};
	};

	struct WindowInitArgs {
		enum Flags {
			NO_DECORATION = 1 << 0,
			NO_TASKBAR_ICON = 1 << 1
		};
		U32 width = 0;
		U32 height = 0;
		const char* name = "";
		bool fullscreen = false;
		U32  flags = 0;
		void* parent = nullptr;
	};
	WindowType CreateCustomWindow(const WindowInitArgs& args);
	void DestroyCustomWindow(WindowType window);
	void SetWindowScreenRect(WindowType win, const WindowRect& rect);
	WindowRect GetWindowScreenRect(WindowType window);
	WindowPoint ToScreen(WindowType window, I32 x, I32 y);
	U32 GetMonitors(Span<Monitor> monitors);
	WindowType GetFocusedWindow();
	void ShowMessageBox(const char* msg);

	bool GetWindowEvent(WindowEvent& event);
	bool IsKeyDown(Keycode key);
	void CreateGuid(void* result);

	MemoryStats GetMemoryStats();
	ProcessMemoryStats GetProcessMemoryStats();

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
	bool   MakeDir(const char* path);
	void   SetCurrentDir(const char* path);
	void   GetCurrentDir(Span<char> path);
	bool   StatFile(const char* path, FileInfo& fileInfo);
	FileIterator* CreateFileIterator(const char* path, const char* ext = nullptr);
	void DestroyFileIterator(FileIterator* it);
	bool GetNextFile(FileIterator* it, ListEntry& info);
	void DebugOutput(const char* msg);
	void Fatal(const char* msg);
	void SetCommandLine(int, char**);
	bool GetCommandLines(Span<char> output);

	struct VULKAN_TEST_API FileSystemWatcher
	{
		virtual ~FileSystemWatcher() {}

		static UniquePtr<FileSystemWatcher> Create(const char* path);
		virtual Delegate<void(const char*)>& GetCallback() = 0;
	};

	enum class SpecialFolder
	{
		Desktop = 0,
		Documents = 1,
		Pictures = 2,
		AppData = 3,
		LocalAppData = 4,
		ProgramData = 5,
		Temporary = 6,
	};
	void GetSpecialFolderPath(SpecialFolder type, Span<char> outputa);

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