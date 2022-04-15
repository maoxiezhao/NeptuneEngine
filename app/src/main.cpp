
#include "client\app\app.h"
#include "core\platform\platform.h"
#include "core\memory\memory.h"
#include "core\utils\delegate.h"
#include "core\utils\profiler.h"
#include "renderer\renderPath3D.h"
#include "renderer\renderer.h"

#include "imgui-docking\imgui.h"
#include "renderer\imguiRenderer.h"

namespace VulkanTest
{
    class MainRenderer : public RenderPath3D 
    {
		void Compose(RenderGraph& renderGraph, GPU::CommandList* cmd)
		{
			RenderPath3D::Compose(renderGraph, cmd);

			ImGuiRenderer::Render(cmd);
		}
    };

    class MainApp : public App
    {
    public:
        void Initialize() override
        {
            App::Initialize();

			ImGuiRenderer::Initialize(*this);
            renderer->ActivePath(&mainRenderer);
        }

		void Uninitialize() override
		{
			ImGuiRenderer::Uninitialize();
			App::Uninitialize();
		}

    protected:
        void Update(F32 deltaTime) override
        {
            PROFILE_BLOCK("Update");
            ImGuiRenderer::BeginFrame();

            engine->Update(*world, deltaTime);

            UpdateGUI(deltaTime);

            ImGuiRenderer::EndFrame();
        }

        bool showDemoWindow = true;
        void UpdateGUI(F32 deltaTime)
        {
            if (showDemoWindow)
                ImGui::ShowDemoWindow(&showDemoWindow);
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

int main(int argc, char* argv[])
{
    return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}