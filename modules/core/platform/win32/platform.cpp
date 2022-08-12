
/////////////////////////////////////////////////////////////////////////////////////////
// PLATFORM WIN32
////////////////////////////////////////////////////////////////////////////////////////
#ifdef CJING3D_PLATFORM_WIN32

#include "platform\platform.h"
#include "core\utils\string.h"
#include "core\profiler\profiler.h"

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

	static void WCharToCharArray(const WCHAR* src, char* dest, int len) 
	{
		for (unsigned int i = 0; i < len / sizeof(WCHAR); ++i) {
			dest[i] = static_cast<char>(src[i]);
		}
		dest[len / sizeof(WCHAR)] = '\0';
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

	struct EventQueue
	{
		struct Rec
		{
			WindowEvent e;
			Rec* next = nullptr;
		};

		void pushBack(const WindowEvent& e)
		{
			Rec* n = CJING_NEW(Rec);
			if (list)
				n->next = list;
			list = n;
			n->e = e;
		}

		WindowEvent popFront()
		{
			ASSERT(list);
			WindowEvent e = list->e;
			Rec* tmp = list;
			list = tmp->next;
			CJING_SAFE_DELETE(tmp);
			return e;
		}

		bool empty() const {
			return !list;
		}

		Rec* list = nullptr;
		DefaultAllocator allocator;
	};

	struct PlatformImpl
	{
		WindowType window = INVALID_WINDOW;
		EventQueue eventQueue;
		WindowPoint relative_mode_pos = {};
		bool relative_mouse = false;
		bool raw_input_registered = false;
		U16 surrogate = 0;
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

	void Uninitialize()
	{
		while (!impl.eventQueue.empty())
			impl.eventQueue.popFront();
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
			// Resource loading thread may cause this case
			// Logger::Error("Current thread dose not set thread index.");
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
		return { (I32)rect.left, (I32)rect.top, (I32)(rect.right - rect.left), (I32)(rect.bottom - rect.top) };
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

	WindowPoint GetMouseScreenPos()
	{
		POINT point;
		static POINT lastPoint = {};
		if (!GetCursorPos(&point))
			point = lastPoint;

		lastPoint = point;
		return { point.x, point.y };
	}

	void SetMouseScreenPos(int x, int y)
	{
		::SetCursorPos(x, y);
	}

	void UpdateGrabbedMouse() 
	{
		if (impl.window == INVALID_WINDOW) {
			ClipCursor(NULL);
			return;
		}

		RECT rect;
		GetWindowRect((HWND)impl.window, &rect);
		ClipCursor(&rect);
	}

	void GrabMouse(WindowType win) {
		impl.window = win;
		UpdateGrabbedMouse();
	}

	WindowType CreateCustomWindow(const WindowInitArgs& args)
	{
		// Create WndClass
		WCharString<MAX_PATH_LENGTH> className("vulkan_window");
		static WNDCLASS wc = [&]() -> WNDCLASS 
		{
			WNDCLASS wc = {};
			auto WndProc = [](HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) -> LRESULT 
			{
				WindowEvent e;
				e.window = hWnd;
				switch (Msg)
				{
				case WM_MOVE:
					e.type = WindowEvent::Type::WINDOW_MOVE;
					e.winMove.x = (I16)LOWORD(lParam);
					e.winMove.y = (I16)HIWORD(lParam);
					impl.eventQueue.pushBack(e);
					UpdateGrabbedMouse();
					return 0;
				case WM_SIZE:
					e.type = WindowEvent::Type::WINDOW_SIZE;
					e.winSize.w = LOWORD(lParam);
					e.winSize.h = HIWORD(lParam);
					impl.eventQueue.pushBack(e);
					UpdateGrabbedMouse();
					return 0;
				case WM_CLOSE:
					e.type = WindowEvent::Type::WINDOW_CLOSE;
					impl.eventQueue.pushBack(e);
					if (hWnd == impl.window) impl.window = INVALID_WINDOW;
					UpdateGrabbedMouse();
					return 0;
				case WM_ACTIVATE:
					if (wParam == WA_INACTIVE) 
					{
						ShowCursor(true);
						GrabMouse(INVALID_WINDOW);
					}

					e.type = WindowEvent::Type::FOCUS;
					e.focus.gained = wParam != WA_INACTIVE;
					impl.eventQueue.pushBack(e);
					UpdateGrabbedMouse();
					break;
				}
				return DefWindowProc(hWnd, Msg, wParam, lParam);
			};

			wc.style = 0;
			wc.lpfnWndProc = WndProc;
			wc.cbClsExtra = 0;
			wc.cbWndExtra = 0;
			wc.hInstance = GetModuleHandle(NULL);
			wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
			wc.hCursor = NULL;
			wc.hbrBackground = NULL;
			wc.lpszClassName = className;

			if (!RegisterClass(&wc)) {
				ASSERT(false);
				return {};
			}
			return wc;
		}();

		// Create window
		DWORD style = args.flags & WindowInitArgs::NO_DECORATION ? WS_POPUP : WS_OVERLAPPEDWINDOW;
		DWORD extStyle = args.flags & WindowInitArgs::NO_TASKBAR_ICON ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
		LONG width = CW_USEDEFAULT; 
		LONG height = CW_USEDEFAULT; 
		if (args.width > 0 && args.height > 0)
		{
			RECT rectangle = {
				0,
				0,
				static_cast<LONG>(args.width),
				static_cast<LONG>(args.height)
			};
			AdjustWindowRect(&rectangle, style, FALSE);

			width = rectangle.right - rectangle.left;
			height = rectangle.bottom - rectangle.top;
		}

		WCharString<MAX_PATH_LENGTH> wname(args.name);
		const HWND hwnd = CreateWindowEx(
			extStyle,
			className,
			wname,
			style,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			width,
			height,
			(HWND)args.parent,
			NULL,
			wc.hInstance,
			NULL);
		ASSERT(hwnd);

		::ShowWindow(hwnd, SW_SHOW);
		::UpdateWindow(hwnd);

		if (!impl.raw_input_registered) 
		{
			RAWINPUTDEVICE device;
			device.usUsagePage = 0x01;
			device.usUsage = 0x02;
			device.dwFlags = RIDEV_INPUTSINK;
			device.hwndTarget = hwnd;
			BOOL status = ::RegisterRawInputDevices(&device, 1, sizeof(device));
			ASSERT(status);
			impl.raw_input_registered = true;
		}

		return hwnd;
	}

	void DestroyCustomWindow(WindowType window)
	{
		::DestroyWindow((HWND)window);
	}

	void SetWindowScreenRect(WindowType win, const WindowRect& rect)
	{
		::MoveWindow((HWND)win, rect.left, rect.top, rect.width, rect.height, TRUE);
	}

	WindowRect GetWindowScreenRect(WindowType window)
	{
		RECT rect;
		::GetWindowRect((HWND)window, &rect);
		return { rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top };
	}

	WindowPoint ToScreen(WindowType window, I32 x, I32 y)
	{
		POINT p;
		p.x = x;
		p.y = y;
		::ClientToScreen((HWND)window, &p);
		return { p.x, p.y };
	}

	static void UTF32ToUTF8(U32 utf32, char* utf8)
	{
		if (utf32 <= 0x7F) 
		{
			utf8[0] = (char)utf32;
		}
		else if (utf32 <= 0x7FF) 
		{
			utf8[0] = 0xC0 | (char)((utf32 >> 6) & 0x1F);
			utf8[1] = 0x80 | (char)(utf32 & 0x3F);
		}
		else if (utf32 <= 0xFFFF) 
		{
			utf8[0] = 0xE0 | (char)((utf32 >> 12) & 0x0F);
			utf8[1] = 0x80 | (char)((utf32 >> 6) & 0x3F);
			utf8[2] = 0x80 | (char)(utf32 & 0x3F);
		}
		else if (utf32 <= 0x10FFFF) 
		{
			utf8[0] = 0xF0 | (char)((utf32 >> 18) & 0x0F);
			utf8[1] = 0x80 | (char)((utf32 >> 12) & 0x3F);
			utf8[2] = 0x80 | (char)((utf32 >> 6) & 0x3F);
			utf8[3] = 0x80 | (char)(utf32 & 0x3F);
		}
		else 
		{
			ASSERT(false);
		}
	}

	U32 GetMonitors(Span<Monitor> monitors)
	{
		struct Callback 
		{
			struct Data 
			{
				Span<Monitor>* monitors;
				U32 index;
			};

			static BOOL CALLBACK func(HMONITOR monitor, HDC, LPRECT, LPARAM lparam)
			{
				Data* data = reinterpret_cast<Data*>(lparam);
				if (data->index >= data->monitors->length()) return TRUE;

				MONITORINFO info = { 0 };
				info.cbSize = sizeof(MONITORINFO);
				if (!::GetMonitorInfo(monitor, &info)) return TRUE;

				Monitor& m = (*data->monitors)[data->index];
				m.monitorRect.left = info.rcMonitor.left;
				m.monitorRect.top = info.rcMonitor.top;
				m.monitorRect.width = info.rcMonitor.right - info.rcMonitor.left;
				m.monitorRect.height = info.rcMonitor.bottom - info.rcMonitor.top;

				m.workRect.left = info.rcWork.left;
				m.workRect.top = info.rcWork.top;
				m.workRect.width = info.rcWork.right - info.rcWork.left;
				m.workRect.height = info.rcWork.bottom - info.rcWork.top;

				m.primary = info.dwFlags & MONITORINFOF_PRIMARY;
				++data->index;

				return TRUE;
			}
		};

		Callback::Data data = {
			&monitors,
			0
		};

		::EnumDisplayMonitors(NULL, NULL, &Callback::func, (LPARAM)&data);
		return data.index;
	}

	WindowType GetFocusedWindow()
	{
		return ::GetActiveWindow();
	}

	void ShowMessageBox(const char* msg)
	{
		WCHAR tmp[2048];
		CharToWChar(tmp, msg);
		MessageBox(NULL, tmp, L"Message", MB_OK);
	}

	bool GetWindowEvent(WindowEvent& event)
	{
		if (!impl.eventQueue.empty()) 
		{
			event = impl.eventQueue.popFront();
			return true;
		}

	retry:
		MSG msg;
		if (!PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) 
			return false;

		event.window = msg.hwnd;

		switch (msg.message) 
		{
		case WM_DROPFILES:
			event.type = WindowEvent::Type::DROP_FILE;
			event.fileDrop.handle = (HDROP)msg.wParam;
			break;
		case WM_QUIT:
			event.type = WindowEvent::Type::QUIT;
			break;
		case WM_CLOSE:
			event.type = WindowEvent::Type::WINDOW_CLOSE;
			break;
		case WM_SYSKEYDOWN:
			if (msg.wParam == VK_MENU) 
				goto retry;
			event.type = WindowEvent::Type::KEY;
			event.key.down = true;
			event.key.keycode = (Keycode)msg.wParam;
			break;
		case WM_SYSCOMMAND:
			if (msg.wParam != SC_KEYMENU || (msg.lParam >> 16) > 0) 
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			goto retry;
		case WM_SYSKEYUP:
			event.type = WindowEvent::Type::KEY;
			event.key.down = false;
			event.key.keycode = (Keycode)msg.wParam;
			break;
		case WM_KEYDOWN:
			event.type = WindowEvent::Type::KEY;
			event.key.down = true;
			event.key.keycode = (Keycode)msg.wParam;
			break;
		case WM_KEYUP:
			event.type = WindowEvent::Type::KEY;
			event.key.down = false;
			event.key.keycode = (Keycode)msg.wParam;
			break;
		case WM_CHAR: {
			event.type = WindowEvent::Type::CHAR;
			event.textInput.utf8 = 0;
			U32 c = (U32)msg.wParam;
			if (c >= 0xd800 && c <= 0xdbff) 
			{
				impl.surrogate = (U16)c;
				goto retry;
			}

			if (c >= 0xdc00 && c <= 0xdfff) 
			{
				if (impl.surrogate) 
				{
					c = (impl.surrogate - 0xd800) << 10;
					c += (WCHAR)msg.wParam - 0xdc00;
					c += 0x10000;
				}
			}
			impl.surrogate = 0;

			UTF32ToUTF8(c, (char*)&event.textInput.utf8);
			break;
		}
		case WM_INPUT: {
			HRAWINPUT hRawInput = (HRAWINPUT)msg.lParam;
			UINT dataSize;
			GetRawInputData(hRawInput, RID_INPUT, NULL, &dataSize, sizeof(RAWINPUTHEADER));
			alignas(RAWINPUT) char dataBuf[1024];
			if (dataSize == 0 || dataSize > sizeof(dataBuf)) break;

			GetRawInputData(hRawInput, RID_INPUT, dataBuf, &dataSize, sizeof(RAWINPUTHEADER));

			const RAWINPUT* raw = (const RAWINPUT*)dataBuf;
			if (raw->header.dwType != RIM_TYPEMOUSE) break;

			const RAWMOUSE& mouseData = raw->data.mouse;
			const USHORT flags = mouseData.usButtonFlags;
			const short wheel_delta = (short)mouseData.usButtonData;
			const LONG x = mouseData.lLastX, y = mouseData.lLastY;

			WindowEvent e;
			if (wheel_delta) 
			{
				e.mouseWheel.amount = (float)wheel_delta / WHEEL_DELTA;
				e.type = WindowEvent::Type::MOUSE_WHEEL;
				impl.eventQueue.pushBack(e);
			}

			if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) 
			{
				e.type = WindowEvent::Type::MOUSE_BUTTON;
				e.mouseButton.button = MouseButton::LEFT;
				e.mouseButton.down = true;
				impl.eventQueue.pushBack(e);
			}
			if (flags & RI_MOUSE_LEFT_BUTTON_UP) 
			{
				e.type = WindowEvent::Type::MOUSE_BUTTON;
				e.mouseButton.button = MouseButton::LEFT;
				e.mouseButton.down = false;
				impl.eventQueue.pushBack(e);
			}

			if (flags & RI_MOUSE_RIGHT_BUTTON_UP) 
			{
				e.type = WindowEvent::Type::MOUSE_BUTTON;
				e.mouseButton.button = MouseButton::RIGHT;
				e.mouseButton.down = false;
				impl.eventQueue.pushBack(e);
			}
			if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) 
			{
				e.type = WindowEvent::Type::MOUSE_BUTTON;
				e.mouseButton.button = MouseButton::RIGHT;
				e.mouseButton.down = true;
				impl.eventQueue.pushBack(e);
			}

			if (flags & RI_MOUSE_MIDDLE_BUTTON_UP) 
			{
				e.type = WindowEvent::Type::MOUSE_BUTTON;
				e.mouseButton.button = MouseButton::MIDDLE;
				e.mouseButton.down = false;
				impl.eventQueue.pushBack(e);
			}
			if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) 
			{
				e.type = WindowEvent::Type::MOUSE_BUTTON;
				e.mouseButton.button = MouseButton::MIDDLE;
				e.mouseButton.down = true;
				impl.eventQueue.pushBack(e);
			}

			if (x != 0 || y != 0) 
			{
				e.type = WindowEvent::Type::MOUSE_MOVE;
				e.mouseMove.xrel = x;
				e.mouseMove.yrel = y;
				impl.eventQueue.pushBack(e);
			}

			if (impl.eventQueue.empty()) 
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				return false;
			}

			event = impl.eventQueue.popFront();
			break;
		}
		default:
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			goto retry;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
		return true;
	}

	bool IsKeyDown(Keycode key)
	{
		const SHORT res = GetAsyncKeyState((int)key);
		return (res & 0x8000) != 0;
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
			// Logger::Warning("Faild to get last mod time \"%s\"", file);
			return 0;
		}

		return attrib.st_mtime;
	}

	bool MakeDir(const char* path)
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

	struct FileSystemWatcherImpl;

	class FileSystemWatcherTask final : public Thread
	{
	public:
		FileSystemWatcherTask(const char* path_, FileSystemWatcherImpl& watcher_) :
			path(path_),
			watcher(watcher_)
		{}

		I32 Task() override;
	
		StaticString<MAX_PATH_LENGTH> path;
		FileSystemWatcherImpl& watcher;
		volatile bool finished = false;
		HANDLE ioHandle;
		DWORD received;
		OVERLAPPED overlapped;
		U8 info[4096];
	};

	struct FileSystemWatcherImpl final : FileSystemWatcher 
	{
		FileSystemWatcherImpl() = default;
		~FileSystemWatcherImpl() override 
		{
			if (task) 
			{
				CancelIoEx(task->ioHandle, nullptr);
				CloseHandle(task->ioHandle);

				task->Destroy();
				CJING_SAFE_DELETE(task);
			}
		}

		bool Start(LPCSTR path) 
		{
			task = CJING_NEW(FileSystemWatcherTask)(path, *this);
			if (!task->Create("Filesystem watcher"))
			{
				CJING_SAFE_DELETE(task);
				task = nullptr;
				return false;
			}
			return true;
		}

		Delegate<void(const char*)>& GetCallback() override { 
			return callback;
		}

		Delegate<void(const char*)> callback;
		FileSystemWatcherTask* task = nullptr;
	};

	static void CALLBACK Notify(DWORD status, DWORD tferred, LPOVERLAPPED over)
	{
		auto* task = (FileSystemWatcherTask*)over->hEvent;
		if (status == ERROR_OPERATION_ABORTED) 
		{
			task->finished = true;
			return;
		}

		if (tferred == 0) 
			return;

		FILE_NOTIFY_INFORMATION* info = (FILE_NOTIFY_INFORMATION*)task->info;
		while (info) 
		{
			auto action = info->Action;
			switch (action) 
			{
			case FILE_ACTION_RENAMED_NEW_NAME:
			case FILE_ACTION_ADDED:
			case FILE_ACTION_MODIFIED:
			case FILE_ACTION_REMOVED: 
			{
				char tmp[MAX_PATH];
				WCharToCharArray(info->FileName, tmp, info->FileNameLength);
				task->watcher.callback.Invoke(tmp);
			} 
			break;
			case FILE_ACTION_RENAMED_OLD_NAME:
				char tmp[MAX_PATH];
				WCharToCharArray(info->FileName, tmp, info->FileNameLength);
				task->watcher.callback.Invoke(tmp);
				break;
			default: 
				ASSERT(false); 
				break;
			}
			info = info->NextEntryOffset == 0 ? nullptr : (FILE_NOTIFY_INFORMATION*)(((U8*)info) + info->NextEntryOffset);
		}
	}

	I32 FileSystemWatcherTask::Task()
	{
		ioHandle = CreateFileA(path.c_str(), 
			FILE_LIST_DIRECTORY, 
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, 
			nullptr, 
			OPEN_EXISTING, 
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, 
			nullptr
		);
		if (ioHandle == INVALID_HANDLE_VALUE)
			return -1;

		memset(&overlapped, 0, sizeof(overlapped));
		overlapped.hEvent = this;
		finished = false;

		while (!finished)
		{
			PROFILE_BLOCK("Watching directory change");
			static const DWORD READ_DIR_CHANGE_FILTER = 
				FILE_NOTIFY_CHANGE_CREATION | 
				FILE_NOTIFY_CHANGE_LAST_WRITE | 
				FILE_NOTIFY_CHANGE_SIZE | 
				FILE_NOTIFY_CHANGE_DIR_NAME | 
				FILE_NOTIFY_CHANGE_FILE_NAME;
			BOOL status = ReadDirectoryChangesW(
				ioHandle, 
				info, sizeof(info), 
				TRUE, 
				READ_DIR_CHANGE_FILTER, 
				&received, 
				&overlapped, 
				&Notify);
			if (status == FALSE) 
				break;

			SleepEx(INFINITE, TRUE); // Wakeup when io finished
		}

		return 0;
	}

	UniquePtr<FileSystemWatcher> FileSystemWatcher::Create(const char* path)
	{
		auto watcher = CJING_MAKE_UNIQUE<FileSystemWatcherImpl>();
		if (!watcher->Start(path))
			return UniquePtr<FileSystemWatcher>();
		return watcher.Move();
	}
}
}
#endif