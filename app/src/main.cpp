
#include "client\app\app.h"
#include "core\platform\platform.h"
#include "core\memory\memory.h"
#include "core\jobsystem\jobsystem.h"
#include "gpu\vulkan\device.h"
#include "math\math.hpp"
#include "renderer\renderPath3D.h"
#include "renderer\renderer.h"

#include "core\utils\delegate.h"

namespace VulkanTest
{
    class MainRenderer : public RenderPath3D {};

    class MainApp : public App
    {
    public:
        U32 GetDefaultWidth() override
        {
            return 1280;
        }

        U32 GetDefaultHeight() override
        {
            return 720;
        }

        void Initialize() override
        {
            App::Initialize();
            renderer->ActivePath(&mainRenderer);
        }

    private:
        MainRenderer mainRenderer;
    };

    App* CreateApplication(int, char**)
    {
        try
        {
            App* app = new MainApp();
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

int test(int aa)
{
    std::cout << aa << std::endl;
    return 0;
}

class AAA
{
public:
    int test(int aa)
    {
        std::cout << aa << std::endl;
        return 0;
    }
};

int main(int argc, char* argv[])
{
    AAA a;
    DelegateList<int(int)> cb;
    cb.Bind<&AAA::test>(&a);
    cb.Bind<test>();
    cb.Invoke(4);
    return 0;
    //return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}