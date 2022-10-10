#include "client\app\app.h"
#include "editor\editor.h"
#include "core\commandLine.h"

namespace VulkanTest
{
    App* CreateApplication(int argc, char** argv)
    {
        char cmdLine[2048];
        Platform::GetCommandLines(cmdLine);
        if (!CommandLine::Parse(cmdLine))
        {
            Logger::Error("Invalid command line.");
            return nullptr;
        }

#ifdef DEBUG_RENDERDOC
        GPU::DeviceVulkan::InitRenderdocCapture();
#endif
        try
        {
            App* app = Editor::EditorApp::Create();
            return app;
        }
        catch (const std::exception& e)
        {
            Logger::Error("CreateApplication() threw exception: %s\n", e.what());
            return nullptr;
        }
    }
}

int main(int argc, char* argv[])
{
    return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}