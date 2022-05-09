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

            auto& meshes = scene.GetMeshes();
            for (auto& kvp : meshes)
            {
                if (vis.frustum.CheckBoxFast(kvp.second->aabb))
                    vis.objects.push_back(kvp.first);
            }
        }
    };

    UniquePtr<CullingSystem> CullingSystem::Create()
    {
        return CJING_MAKE_UNIQUE<CullingSystemImpl>();
    }
}