#include "culling.h"
#include "renderer.h"
#include "renderScene.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{   
    struct CullingTag {};

    class CullingSystemImpl : public CullingSystem
    {
    private:
        ECS::Pipeline pipeline;
        ECS::System cullingObjects;
        Visibility* visibility = nullptr;

    public:
        CullingSystemImpl()
        {
        }

        ~CullingSystemImpl()
        {
        }

        bool Initialize(RenderScene& scene) override
        {
            auto& world = scene.GetWorld();
            // Culling objects
            cullingObjects = world.CreateSystem<const ObjectComponent>()
                .Kind<CullingTag>()
                .MultiThread(true)
                .ForEach([&](ECS::Entity entity, const ObjectComponent& obj) {

                ASSERT(visibility != nullptr);

                if (obj.mesh != ECS::INVALID_ENTITY)
                {
                    if (visibility->frustum.CheckBoxFast(obj.aabb))
                    {
                        I32 index = AtomicIncrement(&visibility->objectCount);
                        visibility->objects[index - 1] = entity;
                    }
                }
                    });

            pipeline = world.CreatePipeline()
                .Term(ECS::EcsCompSystem)
                .Term<CullingTag>()
                .Build();
            return true;
        }

        void Uninitialize() override
        {
            cullingObjects.Destroy();
            pipeline.Destroy();
        }

        void Cull(Visibility& vis, RenderScene& scene) override
        {
            PROFILE_BLOCK("Culling");
            visibility = &vis;
            visibility->frustum = vis.camera->frustum;

            auto& world = scene.GetWorld();
            I32 objCount = world.Count<ObjectComponent>();
            visibility->objects.resize(objCount);

            if (pipeline != ECS::INVALID_ENTITY)
                scene.GetWorld().RunPipeline(pipeline);

            // Finalize visibility
            vis.objects.resize(AtomicRead(&vis.objectCount));
        }
    };

    UniquePtr<CullingSystem> CullingSystem::Create()
    {
        return CJING_MAKE_UNIQUE<CullingSystemImpl>();
    }
}