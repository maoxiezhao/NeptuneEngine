#include "culling.h"
#include "renderer.h"
#include "renderScene.h"

namespace VulkanTest
{   
    class CullingSystemImpl : public CullingSystem
    {
    public:
        void Cull(Visibility& vis, RenderScene& scene) override
        {
            vis.frustum = vis.camera->frustum;

            scene.ForEachObjects([&](ECS::EntityID entity, ObjectComponent& obj) 
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