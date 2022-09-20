#include "textureHelper.h"
#include "renderer.h"
#include "core\platform\sync.h"
#include "core\collections\hashMap.h"
#include "content\resourceManager.h"

namespace VulkanTest
{
	HashMap<U32, ResPtr<Texture>> colorTextures;
	SpinLock colorlock;

	class TextureHelperService : public RendererService
	{
	public:
		TextureHelperService() :
			RendererService("TextureHelper", -700)
		{}

		bool Init(Engine& engine) override
		{
			initialized = true;
			return true;
		}

		void Uninit() override
		{
			colorTextures.clear();
			initialized = false;
		}
	};
	TextureHelperService TextureHelperServiceInstance;

	Texture* TextureHelper::GetColor(const Color4& color)
	{
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

		Texture* texture = CJING_NEW(Texture)(ResourceInfo::Temporary(Texture::ResType));
		if (!texture->Create(1, 1, VK_FORMAT_R8G8B8A8_UNORM, data))
		{
			Logger::Error("Failed to create helper color texture.");
			CJING_SAFE_DELETE(texture);
			return nullptr;
		}
		ResourceManager::AddTemporaryResource(texture);
		
		colorlock.Lock();
		colorTextures.insert(color.GetRGBA(), texture);
		colorlock.Unlock();

		return texture;
	}

}