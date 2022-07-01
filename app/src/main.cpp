#include "client\app\app.h"
#include "editor\editor.h"

namespace VulkanTest
{
    App* CreateApplication(int, char**)
    {
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

using namespace VulkanTest;

int main(int argc, char* argv[])
{
    return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}