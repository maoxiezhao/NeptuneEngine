#pragma once

#include "core\common.h"
#include "content\resources\texture.h"

namespace VulkanTest
{
	class VULKAN_TEST_API SpriteAtlas : public Texture
	{
	public:
		DECLARE_RESOURCE(SpriteAtlas);

		SpriteAtlas(const ResourceInfo& info);
		virtual ~SpriteAtlas();

	protected:
		bool Init(ResourceInitData& initData)override;
		bool Load()override;
		void Unload() override;
	};
}