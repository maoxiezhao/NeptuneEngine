#pragma once

#include "core\common.h"
#include "gpu\vulkan\device.h"
#include "enums.h"

namespace VulkanTest {
	class ResourceManager;
}
namespace VulkanTest::ImageUtil
{
	enum StencilMode
	{
		STENCILMODE_DISABLED,
		STENCILMODE_EQUAL,
		STENCILMODE_LESS,
		STENCILMODE_GREATER,
		STENCILMODE_NOT,
		STENCILMODE_ALWAYS,
		STENCILMODE_COUNT
	};

    struct VULKAN_TEST_API Params
    {
		enum FLAGS
		{
			EMPTY = 0,
			FULLSCREEN = 1 << 0,
		};
		U32 flags = EMPTY;

		F32x3 pos = F32x3(0.0f);
		F32x2 siz = F32x2(1.0f);
		F32x2 scale = F32x2(1.0f);
		F32 rotation = 0;

		F32x2 pivot = F32x2(0.0f);
		F32x2 corners[4] = {
			F32x2(0.0f, 0.0f), F32x2(1.0f, 0.0f),
			F32x2(0.0f, 1.0f), F32x2(1.0f, 1.0f)
		};

        U8 stencilRef = 0;
		StencilMode stencilMode = STENCILMODE_DISABLED;
		BlendMode blendFlag = BLENDMODE_ALPHA;

		constexpr bool IsFullScreenEnabled() const { return flags & FULLSCREEN; }
		constexpr void EnableFullScreen() { flags |= FULLSCREEN; }
    };

    void Initialize(ResourceManager& resManager);
	void Uninitialize();
    void Draw(GPU::Image* image, Params params, GPU::CommandList& cmd);
}