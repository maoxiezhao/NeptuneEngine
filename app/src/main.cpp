#include "client\app\app.h"
#include "core\platform\platform.h"
#include "core\memory\memory.h"
#include "core\utils\delegate.h"
#include "core\utils\profiler.h"
#include "renderer\renderPath3D.h"
#include "renderer\renderer.h"
#include "editor\editor.h"

namespace VulkanTest
{
    App* CreateApplication(int, char**)
    {
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