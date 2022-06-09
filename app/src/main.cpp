#include "client\app\app.h"
#include "core\platform\platform.h"
#include "core\memory\memory.h"
#include "core\utils\delegate.h"
#include "core\utils\profiler.h"
#include "renderer\renderPath3D.h"
#include "renderer\renderer.h"
#include "editor\editor.h"

// TEST
#include "core\collections\hashMap.h"

namespace VulkanTest
{
    class TestRenderer : public RenderPath3D
    {
        void SetupPasses(RenderGraph& renderGraph) override
        {
        }

        void Compose(RenderGraph& renderGraph, GPU::CommandList* cmd) override
        {
            // RenderPath3D::Compose(renderGraph, cmd);
            // Renderer::DrawTest(*cmd);
            // Renderer::BindCameraCB(*camera, *cmd);
            // Renderer::DrawScene(*cmd, visibility);
        }

        void Render() override
        {
            auto device = wsi->GetDevice();

            GPU::CommandListPtr cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);
            cmd->BeginEvent("TriangleTest");
            GPU::RenderPassInfo rp = device->GetSwapchianRenderPassInfo(GPU::SwapchainRenderPassType::ColorOnly);
            cmd->BeginRenderPass(rp);
            {
                cmd->SetDefaultOpaqueState();
                cmd->SetProgram("screenVS.hlsl", "screenPS.hlsl");
                cmd->Draw(3);
            }
            cmd->EndRenderPass();
            cmd->EndEvent();

            device->Submit(cmd);
        };
    };

    class TestApp final : public App
    {
        F32x3 vertices[4] = {
            F32x3(-0.5f, -0.5f, -5.0f),
            F32x3(-0.5f, +0.5f, -5.0f),
            F32x3(+0.5f, +0.5f, 0.0f),
            F32x3(+0.5f, -0.5f, 0.0f),
        };

        U32 indices[6] = {
            0, 1, 2,
            0, 2, 3
        };


    public:
        void Initialize() override
        {
            App::Initialize();
            renderer->ActivePath(&testRenderer);
        
         /*   RenderScene* scene = Renderer::GetScene();
            ECS::EntityID entity = scene->CreateMesh("Test");
            meshComp = scene->GetComponent<MeshComponent>(entity);

            meshComp->vertexPos.resize(4);
            memcpy(meshComp->vertexPos.data(), vertices, sizeof(F32x3) * 4);

            meshComp->indices.resize(6);
            memcpy(meshComp->indices.data(), indices, sizeof(U32) * 6);

            meshComp->subsets.resize(2);
            meshComp->subsets[0].indexCount = 3;
            meshComp->subsets[0].indexOffset = 0;
            meshComp->subsets[1].indexCount = 3;
            meshComp->subsets[1].indexOffset = 3;

            meshComp->SetupRenderData();

            scene->GetMainCamera()->eye = F32x3(1.0f, 0.0f, 1.5f);*/
        }

        void Uninitialize() override
        {
            if (meshComp != nullptr)
            {
                meshComp->vboPos.reset();
                meshComp->ibo.reset();
            }

            App::Uninitialize();
        }

    private:
        TestRenderer testRenderer;
        MeshComponent* meshComp = nullptr;
    };

    App* CreateApplication(int, char**)
    {
        GPU::DeviceVulkan::InitRenderdocCapture();

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