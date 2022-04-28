#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\scene\world.h"

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

        struct CameraComponent* camera = nullptr;
    };

    class VULKAN_TEST_API CullingSystem
    {
    public:
        CullingSystem() = default;
        virtual ~CullingSystem() = default;

        static UniquePtr<CullingSystem> Create();

        virtual void Cull(Visibility& vis, RenderScene& scene) = 0;
    };
}