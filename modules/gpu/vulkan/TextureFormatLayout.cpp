#include "TextureFormatLayout.h"

namespace GPU
{
	void TextureFormatLayout::SetTexture1D(VkFormat format, U32 width, U32 height, U32 arrayLayers, U32 mipLevels)
	{
	}

	void TextureFormatLayout::SetTexture2D(VkFormat format, U32 width, U32 height, U32 arrayLayers, U32 mipLevels)
	{
	}

	void TextureFormatLayout::SetTexture3D(VkFormat format, U32 width, U32 height, U32 arrayLayers, U32 mipLevels)
	{
	}

	U32 TextureFormatLayout::GetRequiredSize() const
	{
		return U32();
	}

	void TextureFormatLayout::BuildBufferImageCopies()
	{
	}
}