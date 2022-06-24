#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;
    class LogWidget;

    class LogWidgetSink : public LoggerSink
    {
    public:
        LogWidgetSink(LogWidget& logUI_);

        void Log(LogLevel level, const char* msg)override;

    private:
        LogWidget& logUI;
    };

    class VULKAN_EDITOR_API LogWidget : public EditorWidget
    {
    public:
        explicit LogWidget();
        virtual ~LogWidget();

        void PushLog(LogLevel level, const char* msg);

        void Update(F32 dt);
        void OnGUI() override;
        const char* GetName();

    private:
        bool isOpen = true;
        LogWidgetSink logSink;
        Mutex mutex;
        U8 levelFilter = 0xff;
        bool scrollTobottom = false;
        bool autoscroll = true;

        I32 newMessageCount[(int)LogLevel::COUNT];

        struct LogMessage
        {
            String text;
            LogLevel level;
        };
        Array<LogMessage> messages;
    };
}
}
