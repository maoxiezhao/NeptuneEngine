#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\scene\world.h"
#include "core\platform\atomic.h"
#include "math\geometry.h"
#include "renderScene.h"

namespace VulkanTest
{
    class RenderScene;

    struct VULKAN_TEST_API Visibility
    {
		enum FLAGS
		{
			EMPTY = 0,
			ALLOW_OBJECTS = 1 << 0,
			ALLOW_LIGHTS = 1 << 1,
			ALLOW_EVERYTHING = ~0u
		};
		U32 flags = EMPTY;

        RenderScene* scene = nullptr;
        struct CameraComponent* camera = nullptr;
        Frustum frustum;

        Array<ECS::Entity> objects;
        Array<ECS::Entity> lights;

        volatile I32 objectCount;

        void Clear()
        {
            objects.clear();
            lights.clear();

            AtomicExchange(&objectCount, 0);
        }
    };

    class VULKAN_TEST_API CullingSystem
    {
    public:
        CullingSystem() = default;
        virtual ~CullingSystem() = default;

        static UniquePtr<CullingSystem> Create();

        virtual bool Initialize(RenderScene& scene) = 0;
        virtual void Uninitialize() = 0;
        virtual void Cull(Visibility& vis, RenderScene& scene) = 0;
    };
}