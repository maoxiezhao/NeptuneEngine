#pragma once

#include "core\common.h"
#include "core\profiler\profiler.h"
#include "fontResource.h"

typedef struct FT_LibraryRec_* FT_Library;

namespace VulkanTest
{
	class Font;
	class FontAltasTexture;

	class VULKAN_TEST_API FontResourceFactory final : public BinaryResourceFactory
	{
	public:
		FontResourceFactory();
		virtual ~FontResourceFactory();

		void Update(F32 dt) override;
		void Uninitialize() override;
		void Flush();

		FT_Library GetLibrary();
		FontAltasTexture* GetAtlas(I32 index);
		bool CreateCharacterInfo(Font* font, I32 c, struct FontCharacterInfo& info);

	protected:
		Resource* CreateResource(const Path& path) override;

		Array<FontAltasTexture*> fontAtlases;
		Mutex mutex;
	};
}