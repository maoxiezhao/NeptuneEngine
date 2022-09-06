#pragma once

#include "core\common.h"
#include "core\profiler\profiler.h"
#include "fontResource.h"

typedef struct FT_LibraryRec_* FT_Library;

namespace VulkanTest
{
	class Font;
	class FontAltasTexture;

	class VULKAN_TEST_API FontManager
	{
	public:
		static void Flush();
		static FT_Library GetLibrary();
		static FontAltasTexture* GetAtlas(I32 index);
		static bool CreateCharacterInfo(Font* font, I32 c, struct FontCharacterInfo& info);
	};
}