#include "culling.h"
#include "renderer.h"
#include "renderScene.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{   
    class CullingSystemImpl : public CullingSystem
    {
    public:
        void Cull(Visibility& vis, RenderScene& scene) override
        {
            PROFILE_FUNCTION();

            vis.frustum = vis.camera->frustum;

            scene.ForEachObjects([&](ECS::Entity entity, ObjectComponent& obj) 
            {
                if (obj.mesh != ECS::INVALID_ENTITY)
                {
                    if (vis.frustum.CheckBoxFast(obj.aabb))
                        vis.objects.push_back(entity);
                }
            });
        }
    };

    UniquePtr<CullingSystem> CullingSystem::Create()
    {
        return CJING_MAKE_UNIQUE<CullingSystemImpl>();
    }
}