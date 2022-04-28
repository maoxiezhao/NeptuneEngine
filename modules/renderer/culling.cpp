#include "culling.h"
#include "renderer.h"
#include "renderScene.h"

namespace VulkanTest
{   
    class CullingSystemImpl : public CullingSystem
    {
    private:

    public:
        void Cull(Visibility& vis, RenderScene& scene) override
        {
            
        }
    };

    UniquePtr<CullingSystem> CullingSystem::Create()
    {
        return CJING_MAKE_UNIQUE<CullingSystemImpl>();
    }
}