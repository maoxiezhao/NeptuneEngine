#include "RenderScene.h"
#include "renderer.h"
#include "culling.h"

namespace VulkanTest
{
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

        virtual ECS::EntityID CreateMesh(const char* name)
        {
            ECS::EntityID entity = world.CreateEntity(name).entity;
            return entity;
        }
    };

    UniquePtr<RenderScene> RenderScene::CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world)
    {
        return CJING_MAKE_UNIQUE<RenderSceneImpl>(rendererPlugin, engine, world);
    }
}