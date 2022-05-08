#include "RenderScene.h"
#include "renderer.h"
#include "culling.h"

namespace VulkanTest
{
    void MeshComponent::SetupRenderData()
    {
        GPU::DeviceVulkan* device = Renderer::GetDevice();

        GPU::BufferCreateInfo bufferInfo = {};
        bufferInfo.domain = GPU::BufferDomain::Device;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.size = vertexPos.size() * sizeof(F32x3);
        vboPos = device->CreateBuffer(bufferInfo, vertexPos.data());

        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufferInfo.size = indices.size() * sizeof(U32);
        ibo = device->CreateBuffer(bufferInfo, indices.data());
    }

    void CameraComponent::UpdateCamera()
    {
        // Construct V, P, VP
    }

    class RenderSceneImpl : public RenderScene
    {
    private:
        Engine& engine;
        World& world;
        RendererPlugin& rendererPlugin;
        CameraComponent mainCamera;
        UniquePtr<CullingSystem> cullingSystem;

        EntityMap<MeshComponent*> meshes;

    public:
        RenderSceneImpl(RendererPlugin& rendererPlugin_, Engine& engine_, World& world_) :
            rendererPlugin(rendererPlugin_),
            engine(engine_),
            world(world_)
        {
            cullingSystem = CullingSystem::Create();
        }

        virtual ~RenderSceneImpl()
        {
            cullingSystem.Reset();
        }

        void Init()override
        {
        }

        void Uninit()override
        {
        }

        void Update(float dt, bool paused)override
        {
        }
        
        void Clear()override
        {
        }

        World& GetWorld()override
        {
            return world;
        }

        IPlugin& GetPlugin()const override
        {
            return rendererPlugin;
        }

        CameraComponent* GetMainCamera()override
        {
            return &mainCamera;
        }

        void UpdateVisibility(struct Visibility& vis)
        {
            cullingSystem->Cull(vis, *this);
        }

        ECS::EntityID CreateMesh(const char* name)override
        {
            return world.CreateEntity(name)
                .With<MeshComponent>()
                .entity;
        }

        EntityMap<MeshComponent*>& GetMeshes()override
        {
            return meshes;
        }
    };

    UniquePtr<RenderScene> RenderScene::CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world)
    {
        return CJING_MAKE_UNIQUE<RenderSceneImpl>(rendererPlugin, engine, world);
    }
}