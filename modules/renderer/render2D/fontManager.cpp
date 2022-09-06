#include "fontManager.h"
#include "font.h"
#include "fontAltasTexture.h"
#include "core\engine.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_MODULE_H            // <freetype/ftmodapi.h>
#include FT_GLYPH_H             // <freetype/ftglyph.h>
#include FT_SYNTHESIS_H         // <freetype/ftsynth.h>
#include FT_BITMAP_H

namespace VulkanTest
{
	namespace
	{
		static void* FreeTypeAlloc(FT_Memory memory, long size)
		{
			return CJING_MALLOC(size);
		}

		void* FreeTypeRealloc(FT_Memory memory, long oldSize, long newSize, void* ptr)
		{
			return CJING_REMALLOC(ptr, newSize);
		}

		void FreeTypeFree(FT_Memory memory, void* ptr)
		{
			return CJING_FREE(ptr);
		}

		Mutex mutex;
		Array<FontAltasTexture*> fontAtlases;
		FT_Library Library;
		FT_MemoryRec_ FreeTypeMemory;
	}

	class FontManagerServiceImpl : public EngineService
	{
	public:


	public:
		FontManagerServiceImpl() :
			EngineService("FontManagerServiceImpl", -700)
		{}

		~FontManagerServiceImpl()
		{
		}

		bool Init(Engine& engine) override
		{
			ASSERT(Library == nullptr);

			FreeTypeMemory.alloc = &FreeTypeAlloc;
			FreeTypeMemory.realloc = &FreeTypeRealloc;
			FreeTypeMemory.free = &FreeTypeFree;
			const FT_Error error = FT_New_Library(&FreeTypeMemory, &Library);
			if (error)
			{
				Logger::Error("Failed to init freetype %x", error);
				return false;
			}

			FT_Add_Default_Modules(Library);

			FT_Int major, minor, patch;
			FT_Library_Version(Library, &major, &minor, &patch);
			Logger::Info("Freetype initialized %d.%d.%d", major, minor, patch);


			const I32 dpi = Platform::GetDPI();
			Font::FontScale	= 1.0f / (dpi / 96.0f);

			initialized = true;
			return true;
		}

		void Update() override
		{
			FontManager::Flush();
		}

		void Uninit() override
		{
			for (auto altas : fontAtlases)
			{
				if (altas)
					altas->Destroy();
			}
			fontAtlases.clear();

			if (Library)
			{
				const FT_Error error = FT_Done_Library(Library);
				Library = nullptr;
				if (error)
				{
					Logger::Error("Failed to uninit freetype %x", error);
				}
			}
			initialized = false;
		}
	};
	FontManagerServiceImpl FontManagerServiceImplInstance;

	void FontManager::Flush()
	{
		for (auto altas : fontAtlases)
			altas->Flush();
	}

	FT_Library FontManager::GetLibrary()
	{
		return Library;
	}

	FontAltasTexture* FontManager::GetAtlas(I32 index)
	{
		return index >= 0 && index < (I32)fontAtlases.size() ? fontAtlases[index] : nullptr;
	}

	bool FontManager::CreateCharacterInfo(Font* font, I32 c, FontCharacterInfo& info)
	{
		ScopedMutex lock(mutex);

		// Flush font size
		font->FlushFaceSize();

		FontResource* fontRes = font->GetResource();
		const FT_Face face = fontRes->GetFace();

		U32 glyphFlags = FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_NORMAL;

		// Get the index to the glyph in the font face
		const FT_UInt glyphIndex = FT_Get_Char_Index(face, c);

		// Load the glyph
		FT_Error error = FT_Load_Glyph(face, glyphIndex, glyphFlags);
		if (error)
		{
			Logger::Error("FreeType error:%x", error);
			return false;
		}

		// Render glyph to the bitmap
		FT_GlyphSlot slot = face->glyph;
		error = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
		if (error)
		{
			Logger::Error("FreeType error:%x", error);
			return false;
		}

		FT_Bitmap tmpBitmap;
		FT_Bitmap* bitmap = &slot->bitmap;
		if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY)
		{
			// Convert the bitmap to 8bpp grayscale
			FT_Bitmap_New(&tmpBitmap);
			FT_Bitmap_Convert(Library, bitmap, &tmpBitmap, 4);
			bitmap = &tmpBitmap;
		}
		ASSERT(bitmap && bitmap->pixel_mode == FT_PIXEL_MODE_GRAY);

		// Fill the character info
		memset(&info, 0, sizeof(FontCharacterInfo));
		info.character = c;
		info.offsetX = slot->bitmap_left;
		info.offsetY = slot->bitmap_top;
		info.advanceX = Convert26Dot6ToRoundedPixel<I16>(slot->advance.x);
		info.height = Convert26Dot6ToRoundedPixel<I16>(slot->metrics.height);
		info.isValid = true;

		// Allocate memory
		const I32 glyphWidth = bitmap->width;
		const I32 glyphHeight = bitmap->rows;

		Array<U8> glyphData;
		glyphData.resize(glyphWidth * glyphHeight);

		// Return true if is empty glyph
		if (glyphData.empty())
		{
			info.textureIndex = -1;
			if (&tmpBitmap == bitmap)
				FT_Bitmap_Done(Library, bitmap);

			return true;
		}

		for (I32 row = 0; row < glyphHeight; row++)
			memcpy(&glyphData[row * glyphWidth], &bitmap->buffer[row * bitmap->pitch], glyphWidth);

		if (bitmap->num_grays != 256)
		{
			ASSERT(false);
		}

		// Free temporary bitmap if used
		if (bitmap == &tmpBitmap)
		{
			FT_Bitmap_Done(Library, bitmap);
			bitmap = nullptr;
		}

		// Find target font altas
		const FontAltasTexture::Slot* packSlot = nullptr;
		I32 altasIndex = 0;
		for (; altasIndex < (I32)fontAtlases.size(); altasIndex++)
		{
			packSlot = fontAtlases[altasIndex]->AddCharacter(glyphWidth, glyphHeight, glyphData);
			if (packSlot)
				break;
		}

		// No avaiable altas for current character, create new altas
		if (packSlot == nullptr)
		{
			FontAltasTexture* altas = CJING_NEW(FontAltasTexture)(
				VK_FORMAT_R8_UNORM, 
				FontAltasTexture::PadWithZero,  
				altasIndex,
				fontRes->GetResourceManager()
			);
			altas->Init(512, 512);
			fontAtlases.push_back(altas);

			packSlot = altas->AddCharacter(glyphWidth, glyphHeight, glyphData);
		}

		if (packSlot == nullptr)
		{
			Logger::Error("Failed to find a altas for character %d", c);
			return false;
		}

		const U32 padding = fontAtlases[altasIndex]->GetPadding();
		info.textureIndex = altasIndex;
		info.uv.x = (F32)(packSlot->x + padding);
		info.uv.y = (F32)(packSlot->y + padding);
		info.uvSize.x = (F32)(packSlot->w - 2 * padding);
		info.uvSize.y = (F32)(packSlot->h - 2 * padding);

		return true;
	}
}