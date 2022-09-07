#include "textureHelper.h"
#include "core\platform\sync.h"
#include "core\collections\hashMap.h"
#include "core\resource\resourceManager.h"

namespace VulkanTest
{
	ResourceManager* resourceManager = nullptr;
	HashMap<U32, ResPtr<Texture>> colorTextures;
	SpinLock colorlock;

	void TextureHelper::Initialize(ResourceManager* resourceManager_)
	{
		resourceManager = resourceManager_;
	}

	void TextureHelper::Uninitialize()
	{
		colorTextures.clear();
		resourceManager = nullptr;
	}

	Texture* TextureHelper::GetColor(const Color4& color)
	{
		if (resourceManager == nullptr)
			return nullptr;

		colorlock.Lock();
		auto it = colorTextures.find(color.GetRGBA());
		colorlock.Unlock();

		if (it.isValid())
			return it.value();

		U8 data[4];
		for (int i = 0; i < ARRAYSIZE(data); i += 4)
		{
			data[i] = color.GetR();
			data[i + 1] = color.GetG();
			data[i + 2] = color.GetB();
			data[i + 3] = color.GetA();
		}

		Texture* texture = CJING_NEW(Texture)(ResourceInfo::Temporary(Texture::ResType), *resourceManager);
		if (!texture->Create(1, 1, VK_FORMAT_R8G8B8A8_UNORM, data))
		{
			Logger::Error("Failed to create helper color texture.");
			CJING_SAFE_DELETE(texture);
			return nullptr;
		}
		resourceManager->AddTemporaryResource(texture);
		
		colorlock.Lock();
		colorTextures.insert(color.GetRGBA(), texture);
		colorlock.Unlock();

		return texture;
	}

}