#include "font.h"

#include "ft2build.h"
#include FT_FREETYPE_H

namespace VulkanTest
{
	Font::Font(FontResource* res_, I32 size_) :
		res(res_),
		size(size_)
	{
	}

	Font::~Font()
	{
	}
}