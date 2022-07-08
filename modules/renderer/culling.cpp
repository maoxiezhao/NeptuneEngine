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

            scene.ForEachMeshes([&](ECS::EntityID entity, MeshComponent& mesh) {
                if (mesh.model)
                {
                    if (vis.frustum.CheckBoxFast(mesh.aabb))
                        vis.meshes.push_back(entity);
                }
             });
        }
    };

    UniquePtr<CullingSystem> CullingSystem::Create()
    {
        return CJING_MAKE_UNIQUE<CullingSystemImpl>();
    }
}