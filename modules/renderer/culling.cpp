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
        ECS::System cullingLights;
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

            // Culling lights
            cullingLights = world.CreateSystem<const LightComponent>()
                .Kind<CullingTag>()
                .MultiThread(true)
                .ForEach([&](ECS::Entity entity, const LightComponent& light) {

                ASSERT(visibility != nullptr);

                if (visibility->frustum.CheckBoxFast(light.aabb))
                {
                    I32 index = AtomicIncrement(&visibility->lightCount);
                    visibility->lights[index - 1] = entity;
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
            cullingLights.Destroy();
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

            I32 lightCount = world.Count<LightComponent>();
            visibility->lights.resize(lightCount);

            if (pipeline != ECS::INVALID_ENTITY)
                scene.GetWorld().RunPipeline(pipeline);

            // Finalize visibility
            vis.objects.resize(AtomicRead(&vis.objectCount));
            vis.lights.resize(AtomicRead(&vis.lightCount));
        }
    };

    UniquePtr<CullingSystem> CullingSystem::Create()
    {
        return CJING_MAKE_UNIQUE<CullingSystemImpl>();
    }
}