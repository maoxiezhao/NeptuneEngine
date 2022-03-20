#include "RenderScene.h"
#include "renderer.h"

namespace VulkanTest
{
    class RenderSceneImpl : public RenderScene
    {
    private:
        Engine& engine;
        World& world;

    public:
        RenderSceneImpl(Engine& engine_, World& world_) :
            engine(engine_),
            world(world_)
        {
        }

        virtual ~RenderSceneImpl()
        {
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
    };

    UniquePtr<RenderScene> RenderScene::CreateScene(Engine& engine, World& world)
    {
        return CJING_MAKE_UNIQUE<RenderSceneImpl>(engine, world);
    }
}