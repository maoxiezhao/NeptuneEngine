#define NOGDI
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma warning (push)
#pragma warning (disable: 4091) // declaration of 'xx' hides previous local declaration
#include <DbgHelp.h>
#pragma warning (pop)

#pragma comment(lib, "DbgHelp.lib")

#include "core\platform\debug.h"
#include "core\platform\platform.h"
#include "core\utils\string.h"
#include "core\utils\path.h"

namespace VulkanTest
{
	static bool crashReportingEnabled = false;

	static void GetStack(CONTEXT& context, Span<char> out)
	{
		BOOL result;
		STACKFRAME64 stack;
		alignas(IMAGEHLP_SYMBOL64) char symbol_mem[sizeof(IMAGEHLP_SYMBOL64) + 256];
		IMAGEHLP_SYMBOL64* symbol = (IMAGEHLP_SYMBOL64*)symbol_mem;
		char name[256];
		CopyString(out, "Crash callstack:\n");
		memset(&stack, 0, sizeof(STACKFRAME64));

		HANDLE process = GetCurrentProcess();
		HANDLE thread = GetCurrentThread();
		DWORD64 displacement = 0;
		DWORD machineType;

#ifdef _M_X64
		machineType = IMAGE_FILE_MACHINE_AMD64;
		stack.AddrPC.Offset = context.Rip;
		stack.AddrPC.Mode = AddrModeFlat;
		stack.AddrStack.Offset = context.Rsp;
		stack.AddrStack.Mode = AddrModeFlat;
		stack.AddrFrame.Offset = context.Rbp;
		stack.AddrFrame.Mode = AddrModeFlat;
#else
#error not supported
		machineType = IMAGE_FILE_MACHINE_I386;
		stack.AddrPC.Offset = context.Eip;
		stack.AddrPC.Mode = AddrModeFlat;
		stack.AddrStack.Offset = context.Esp;
		stack.AddrStack.Mode = AddrModeFlat;
		stack.AddrFrame.Offset = context.Ebp;
		stack.AddrFrame.Mode = AddrModeFlat;
#endif

		do
		{
			// Obtains a stack trace
			// See: https://docs.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-stackwalk
			result = StackWalk64(machineType,
				process,
				thread,
				&stack,
				&context,
				NULL,
				SymFunctionTableAccess64,
				SymGetModuleBase64,
				NULL);

			symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
			symbol->MaxNameLength = 255;

			BOOL sybol_valid = SymGetSymFromAddr64(process, (ULONG64)stack.AddrPC.Offset, &displacement, symbol);
			auto err = GetLastError();
			DWORD num_char = UnDecorateSymbolName(symbol->Name, (PSTR)name, 256, UNDNAME_COMPLETE);

			if (!CatString(out, symbol->Name)) return;
			if (!CatString(out, "\n")) return;

		} while (result);
	}

	bool SendEmailFile(LPCSTR lpszSubject,
		LPCSTR lpszTo,
		LPCSTR lpszName,
		LPCSTR lpszText,
		LPCSTR lpszFullFileName)
	{
		return TRUE;
	}

	// Create dump file and report it by sending email
	static LONG WINAPI UnhandledExceptionHandler(LPEXCEPTION_POINTERS info)
	{
		if (!crashReportingEnabled) return EXCEPTION_CONTINUE_SEARCH;

		HANDLE process = GetCurrentProcess();
		SymInitialize(process, nullptr, TRUE);
		struct CrashInfo
		{
			LPEXCEPTION_POINTERS info;
			DWORD threadID;
		};

		auto dumper = [](void* data) -> DWORD 
		{
			///////////////////////////////////////////////////////////////////////////////////////////
			// Get stack infos
			LPEXCEPTION_POINTERS info = ((CrashInfo*)data)->info;
			U64 base = (U64)GetModuleHandle(NULL);
			StaticString<4096> message;
			if (info)
			{
				GetStack(*info->ContextRecord, Span(message.data));
				message << "\nCode: " << (U32)info->ExceptionRecord->ExceptionCode;
				message << "\nAddress: " << (U64)info->ExceptionRecord->ExceptionAddress;
				message << "\nBase: " << (U64)base;
				
				Platform::ShowMessageBox(message);
			}
			else
			{
				message.data[0] = '\0';
			}

			///////////////////////////////////////////////////////////////////////////////////////////
			// Create minidump
			char minidumpPath[MAX_PATH_LENGTH];
			GetCurrentDirectoryA(sizeof(minidumpPath), minidumpPath);
			CatString(minidumpPath, "\\minidump.dmp");
			HANDLE process = GetCurrentProcess();
			DWORD processID = GetProcessId(process);
			HANDLE file = CreateFileA(minidumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			MINIDUMP_TYPE minidumpType = (MINIDUMP_TYPE)(
				MiniDumpWithFullMemoryInfo | MiniDumpFilterMemory | MiniDumpWithHandleData |
				MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);

			MINIDUMP_EXCEPTION_INFORMATION minidump_exception_info;
			minidump_exception_info.ThreadId = ((CrashInfo*)data)->threadID;
			minidump_exception_info.ExceptionPointers = info;
			minidump_exception_info.ClientPointers = FALSE;

			// Writes user-mode minidump information to the specified file
			// See:https://docs.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-minidumpwritedump
			MiniDumpWriteDump(process,
				processID,
				file,
				minidumpType,
				info ? &minidump_exception_info : nullptr,
				nullptr,
				nullptr);
			CloseHandle(file);

			///////////////////////////////////////////////////////////////////////////////////////////
			// Create minidump with full infos
			minidumpType = (MINIDUMP_TYPE)(MiniDumpWithFullMemory | MiniDumpWithFullMemoryInfo |
				MiniDumpFilterMemory | MiniDumpWithHandleData |
				MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
			file = CreateFile(L"fulldump.dmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			MiniDumpWriteDump(process,
				processID,
				file,
				minidumpType,
				info ? &minidump_exception_info : nullptr,
				nullptr,
				nullptr);

			CloseHandle(file);

			///////////////////////////////////////////////////////////////////////////////////////////
			// Send email
			SendEmailFile(
				"Vulkan_Test crash",
				"SMTP:844635471@qq.com",
				"ZZZZyyy",
				message,
				minidumpPath);
			return 0;
		};

		DWORD threadID;
		CrashInfo crashInfo = { info, GetCurrentThreadId() };
		auto handle = CreateThread(0, 0x8000, dumper, &crashInfo, 0, &threadID);
		WaitForSingleObject(handle, INFINITE);

		StaticString<4096> message;
		GetStack(*info->ContextRecord, Span(message.data));
		Logger::Error(message);

		return EXCEPTION_CONTINUE_SEARCH;
	}

	void VULKAN_TEST_API SetupUnhandledExceptionHandler()
	{
		SetUnhandledExceptionFilter(UnhandledExceptionHandler);
	}
}