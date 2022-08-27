#include "font.h"
#include "fontManager.h"

#include "ft2build.h"
#include FT_FREETYPE_H

namespace VulkanTest
{
	F32 Font::FontScale = 1.0f;

	Font::Font(FontResource* res_, I32 size_) :
		res(res_),
		size(size_)
	{
		FlushFaceSize();
		const FT_Face face = res->face;
		ASSERT(face != nullptr);
		height = Convert26Dot6ToRoundedPixel<I32>(FT_MulFix(face->height, face->size->metrics.y_scale));
		ascender = Convert26Dot6ToRoundedPixel<I16>(face->size->metrics.ascender);
		descender = Convert26Dot6ToRoundedPixel<I16>(face->size->metrics.descender);
		hasKerning = FT_HAS_KERNING(face) != 0;
	}

	Font::~Font()
	{
		if (res != nullptr)
			res->fonts.erase(this);
	}

	void Font::FlushFaceSize() const
	{
		const FT_Face face = res->face;
		ASSERT(face != nullptr);

		// Set the character size
		F32 charSize = (F32)size * Font::FontScale;
		const FT_Error error = FT_Set_Char_Size(face, 0, ConvertPixelTo26Dot6<FT_F26Dot6>(charSize), 96, 96);
		if (error)
			Logger::Error("Freetype error:%x", error);

		// Clear transform
		FT_Set_Transform(face, nullptr, nullptr);
	}

	void Font::GetCharacter(I32 c, FontCharacterInfo& info)
	{
		auto it = characterInfos.find(c);
		if (it.isValid())
		{
			info = it.value();
			return;
		}

		ScopedMutex lock(res->mutex);

		// More than one thread wants to get the same character
		it = characterInfos.find(c);
		if (it.isValid())
		{
			info = it.value();
			return;
		}

		// Create new character info
		FontResourceFactory* factory = static_cast<FontResourceFactory*>(&res->GetResourceFactory());
		factory->CreateCharacterInfo(this, c, info);
		characterInfos.insert(c, info);
	}

	I32 Font::GetKerning(I32 first, I32 second) const
	{
		if (hasKerning == false)
			return 0;

		const U32 key = (U32)first << 16 | second;
		auto it = kerningTable.find(key);
		if (it.isValid())
			return it.value();

		ScopedMutex lock(res->mutex);
		
		// More than one thread wants to get the same character
		it = kerningTable.find(key);
		if (it.isValid())
			return it.value();

		FlushFaceSize();

		const FT_Face face = res->face;
		FT_Vector vec;
		const FT_UInt firstIndex = FT_Get_Char_Index(face, first);
		const FT_UInt secondIndex = FT_Get_Char_Index(face, second);
		FT_Get_Kerning(face, firstIndex, secondIndex, FT_KERNING_DEFAULT, &vec);
		I32 kerning = vec.x >> 6;
		kerningTable.insert(key, kerning);
		return kerning;
	}
}