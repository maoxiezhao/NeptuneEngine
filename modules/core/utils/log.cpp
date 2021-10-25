#include "log.h"
#include "platform\platform.h"

#include <mutex>
#include <stdarg.h>
#include <iostream>
#include <vector>
#include <array>

namespace Logger
{
	namespace
	{
#ifdef CJING3D_PLATFORM_WIN32
		enum ConsoleFontColor
		{
			CONSOLE_FONT_WHITE,
			CONSOLE_FONT_BLUE,
			CONSOLE_FONT_YELLOW,
			CONSOLE_FONT_GREEN,
			CONSOLE_FONT_RED
		};

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
#endif

		struct LogContext
		{
			static const int BUFFER_SIZE = 64 * 1024;
			std::array<char, BUFFER_SIZE> buffer_ = {};

			LogContext() {
				buffer_.fill('\0');
			}
		};
		thread_local LogContext mLogContext;

		struct LoggerImpl
		{
			std::mutex mMutex;
			bool mDisplayTime = false;
			std::vector<LoggerSink*> mSinks; // DynamicArray<LoggerSink*>
		};
		static LoggerImpl mImpl;

		void LogImpl(LogLevel level, const char* msg, va_list args)
		{
			vsprintf_s(mLogContext.buffer_.data(), mLogContext.buffer_.size(), msg, args);
			{
				std::lock_guard lock(mImpl.mMutex);
				for (auto sink : mImpl.mSinks) {
					sink->Log(level, mLogContext.buffer_.data());
				}
			}
			mLogContext.buffer_.fill('\0');
		}
	}
	
	void SetIsDisplayTime(bool displayTime)
	{
		mImpl.mDisplayTime = displayTime;
	}

	bool IsDisplayTime()
	{
		return mImpl.mDisplayTime;
	}

	void RegisterSink(LoggerSink& sink)
	{
		std::lock_guard lock(mImpl.mMutex);
		for (auto ptr : mImpl.mSinks) 
		{
			if (ptr == &sink) {
				return;
			}
		}
		mImpl.mSinks.push_back(&sink);
	}

	void UnregisterSink(LoggerSink& sink)
	{
		std::lock_guard lock(mImpl.mMutex);
		for (auto it = mImpl.mSinks.begin(); it != mImpl.mSinks.end(); it++)
		{
			if ((*it) == &sink) 
			{
				mImpl.mSinks.erase(it);
				break;
			}
		}
	}

	void Log(LogLevel level, const char* msg, ...)
	{
		va_list args;
		va_start(args, msg);
		LogImpl(level, msg, args);
		va_end(args);
	}

	void Print(const char* msg, ...)
	{
		va_list args;
		va_start(args, msg);
		LogImpl(LogLevel::LVL_DEV, msg, args);
		va_end(args);
	}

	void Info(const char* msg, ...)
	{
		va_list args;
		va_start(args, msg);
		LogImpl(LogLevel::LVL_INFO, msg, args);
		va_end(args);
	}

	void Warning(const char* msg, ...)
	{
		va_list args;
		va_start(args, msg);
		LogImpl(LogLevel::LVL_WARNING, msg, args);
		va_end(args);
	}

	void Error(const char* msg, ...)
	{
		va_list args;
		va_start(args, msg);
		LogImpl(LogLevel::LVL_ERROR, msg, args);
		va_end(args);
	}

	const char* GetPrefix(LogLevel level)
	{
		const char* prefix = "";
		switch (level)
		{
		case LogLevel::LVL_INFO:
			prefix = "[Info]";
			break;
		case LogLevel::LVL_WARNING:
			prefix = "[Warning]";
			break;
		case LogLevel::LVL_ERROR:
			prefix = "[Error]";
			break;
		}
		return prefix;
	}
}

void StdoutLoggerSink::Log(LogLevel level, const char* msg)
{
	switch (level)
	{
	case LogLevel::LVL_DEV:
		Platform::SetLoggerConsoleFontColor(Platform::CONSOLE_FONT_WHITE);
		break;
	case LogLevel::LVL_INFO:
		Platform::SetLoggerConsoleFontColor(Platform::CONSOLE_FONT_GREEN);
		break;
	case LogLevel::LVL_WARNING:
		Platform::SetLoggerConsoleFontColor(Platform::CONSOLE_FONT_YELLOW);
		break;
	case LogLevel::LVL_ERROR:
		Platform::SetLoggerConsoleFontColor(Platform::CONSOLE_FONT_RED);
		break;
	}

	std::cout << Logger::GetPrefix(level) << " ";
	std::cout << msg << std::endl;
	Platform::SetLoggerConsoleFontColor(Platform::CONSOLE_FONT_WHITE);
}
