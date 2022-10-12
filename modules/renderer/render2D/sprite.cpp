#include "sprite.h"

namespace VulkanTest
{
	REGISTER_BINARY_RESOURCE(SpriteAtlas);

	SpriteAtlas::SpriteAtlas(const ResourceInfo& info) :
		Texture(info)
	{
	}

	SpriteAtlas::~SpriteAtlas()
	{
	}

	bool SpriteAtlas::Init(ResourceInitData& initData)
	{
		return false;
	}

	bool SpriteAtlas::Load()
	{
		return false;
	}

	void SpriteAtlas::Unload()
	{
	}
}