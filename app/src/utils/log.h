#pragma once

enum class LogLevel
{
	LVL_DEV,
	LVL_INFO,
	LVL_WARNING,
	LVL_ERROR,
	COUNT
};

class LoggerSink
{
public:
	virtual ~LoggerSink() {}
	virtual void Log(LogLevel level, const char* msg) = 0;
};

namespace Logger
{
	void SetIsDisplayTime(bool displayTime);
	bool IsDisplayTime();
	void RegisterSink(LoggerSink& sink);
	void UnregisterSink(LoggerSink& sink);

	void Log(LogLevel level, const char* msg, ...);
	void Print(const char* msg, ...);
	void Info(const char* msg, ...);
	void Warning(const char* msg, ...);
	void Error(const char* msg, ...);
	const char* GetPrefix(LogLevel level);
}

class StdoutLoggerSink : public LoggerSink
{
public:
	void Log(LogLevel level, const char* msg)override;
};