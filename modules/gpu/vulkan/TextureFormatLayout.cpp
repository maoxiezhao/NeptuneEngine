#include "TextureFormatLayout.h"
#include <array>

namespace VulkanTest
{
namespace GPU
{
	FormatInfo TextureFormatLayout::GetFormatInfo(VkFormat format, VkImageAspectFlags aspect) const
	{
		FormatInfo info = {};
#define fmt(x)						\
    case VK_FORMAT_##x:				\

		switch (format)
		{
			fmt(R64G64B64A64_UINT)
			fmt(R64G64B64A64_SINT)
			fmt(R64G64B64A64_SFLOAT)
				info.rBits = 64;
				info.gBits = 64;
				info.bBits = 64;
				info.aBits = 64;
				break;
			fmt(R64G64B64_UINT)
			fmt(R64G64B64_SINT)
			fmt(R64G64B64_SFLOAT)
				info.rBits = 64;
				info.gBits = 64;
				info.bBits = 64;
				break;
			fmt(R64G64_UINT)
			fmt(R64G64_SINT)
			fmt(R64G64_SFLOAT)
				info.rBits = 64;
				info.gBits = 64;
				break;
			fmt(R64_UINT)
			fmt(R64_SINT)
			fmt(R64_SFLOAT)
				info.rBits = 64;
				break;

			fmt(R32G32B32A32_UINT)
			fmt(R32G32B32A32_SINT)
			fmt(R32G32B32A32_SFLOAT)
				info.rBits = 32;
				info.gBits = 32;
				info.bBits = 32;
				info.aBits = 32;
				break;
			fmt(R32G32B32_UINT)
			fmt(R32G32B32_SINT)
			fmt(R32G32B32_SFLOAT)
				info.rBits = 32;
				info.gBits = 32;
				info.bBits = 32;
				break;
			fmt(R32G32_UINT)
			fmt(R32G32_SINT)
			fmt(R32G32_SFLOAT)
				info.rBits = 32;
				info.gBits = 32;
				break;
			fmt(R32_UINT)
			fmt(R32_SINT)
			fmt(R32_SFLOAT)
				info.rBits = 32;
				break;

			fmt(R16G16B16A16_UNORM)
			fmt(R16G16B16A16_SNORM)
			fmt(R16G16B16A16_USCALED)
			fmt(R16G16B16A16_SSCALED)
			fmt(R16G16B16A16_UINT)
			fmt(R16G16B16A16_SINT)
			fmt(R16G16B16A16_SFLOAT)
				info.rBits = 16;
				info.gBits = 16;
				info.bBits = 16;
				info.aBits = 16;
				break;
			fmt(R16G16B16_UNORM)
			fmt(R16G16B16_SNORM)
			fmt(R16G16B16_USCALED)
			fmt(R16G16B16_SSCALED)
			fmt(R16G16B16_UINT)
			fmt(R16G16B16_SINT)
			fmt(R16G16B16_SFLOAT)
				info.rBits = 16;
				info.gBits = 16;
				info.bBits = 16;
				break;
			fmt(R16G16_UNORM)
			fmt(R16G16_SNORM)
			fmt(R16G16_USCALED)
			fmt(R16G16_SSCALED)
			fmt(R16G16_UINT)
			fmt(R16G16_SINT)
			fmt(R16G16_SFLOAT)
				info.rBits = 16;
				info.gBits = 16;
				break;
			fmt(R16_UINT)
			fmt(R16_SINT)
			fmt(R16_SFLOAT)
			fmt(R16_UNORM)
			fmt(R16_SNORM)
			fmt(R16_USCALED)
			fmt(R16_SSCALED)
				info.rBits = 16;
				break;

			fmt(R8G8B8A8_UNORM)
			fmt(R8G8B8A8_SNORM)
			fmt(R8G8B8A8_USCALED)
			fmt(R8G8B8A8_SSCALED)
			fmt(R8G8B8A8_UINT)
			fmt(R8G8B8A8_SINT)
			fmt(R8G8B8A8_SRGB)
			fmt(B8G8R8A8_UNORM)
			fmt(B8G8R8A8_SNORM)
			fmt(B8G8R8A8_USCALED)
			fmt(B8G8R8A8_SSCALED)
			fmt(B8G8R8A8_UINT)
			fmt(B8G8R8A8_SINT)
			fmt(B8G8R8A8_SRGB)
			fmt(A8B8G8R8_UNORM_PACK32)
			fmt(A8B8G8R8_SNORM_PACK32)
			fmt(A8B8G8R8_USCALED_PACK32)
			fmt(A8B8G8R8_SSCALED_PACK32)
			fmt(A8B8G8R8_UINT_PACK32)
			fmt(A8B8G8R8_SINT_PACK32)
			fmt(A8B8G8R8_SRGB_PACK32)
				info.rBits = 8;
				info.gBits = 8;
				info.bBits = 8;
				info.aBits = 8;
				break;
			fmt(R8G8B8_UNORM)
			fmt(R8G8B8_SNORM)
			fmt(R8G8B8_USCALED)
			fmt(R8G8B8_SSCALED)
			fmt(R8G8B8_UINT)
			fmt(R8G8B8_SINT)
			fmt(R8G8B8_SRGB)
				info.rBits = 8;
				info.gBits = 8;
				info.bBits = 8;
				break;
			fmt(R8G8_UNORM)
			fmt(R8G8_SNORM)
			fmt(R8G8_USCALED)
			fmt(R8G8_SSCALED)
			fmt(R8G8_UINT)
			fmt(R8G8_SINT)
			fmt(R8G8_SRGB)
				info.rBits = 8;
				info.gBits = 8;
				break;
			fmt(R8_UNORM)
			fmt(R8_SNORM)
			fmt(R8_USCALED)
			fmt(R8_SSCALED)
			fmt(R8_UINT)
			fmt(R8_SINT)
			fmt(R8_SRGB)
				info.rBits = 8;
				break;

			fmt(A2B10G10R10_UNORM_PACK32)
			fmt(A2B10G10R10_SNORM_PACK32)
			fmt(A2B10G10R10_USCALED_PACK32)
			fmt(A2B10G10R10_SSCALED_PACK32)
			fmt(A2B10G10R10_UINT_PACK32)
			fmt(A2B10G10R10_SINT_PACK32)
			fmt(A2R10G10B10_UNORM_PACK32)
			fmt(A2R10G10B10_SNORM_PACK32)
			fmt(A2R10G10B10_USCALED_PACK32)
			fmt(A2R10G10B10_SSCALED_PACK32)
			fmt(A2R10G10B10_UINT_PACK32)
			fmt(A2R10G10B10_SINT_PACK32)
				info.aBits = 2;
				info.rBits = 10;
				info.gBits = 10;
				info.bBits = 10;
			break;
			fmt(R4G4B4A4_UNORM_PACK16)
			fmt(B4G4R4A4_UNORM_PACK16)
				info.aBits = 4;
				info.rBits = 4;
				info.gBits = 4;
				info.bBits = 4;
				break;
			fmt(R5G6B5_UNORM_PACK16)
			fmt(B5G6R5_UNORM_PACK16)
				info.rBits = 5;
				info.gBits = 6;
				info.bBits = 5;
				break;
			fmt(R5G5B5A1_UNORM_PACK16)
			fmt(B5G5R5A1_UNORM_PACK16)
			fmt(A1R5G5B5_UNORM_PACK16)
				info.aBits = 1;
				info.rBits = 5;
				info.gBits = 5;
				info.bBits = 5;
				break;
			fmt(R4G4_UNORM_PACK8)
				info.rBits = 4;
				info.gBits = 4;
				break;

			fmt(B10G11R11_UFLOAT_PACK32)
				info.rBits = 11;
				info.gBits = 11;
				info.bBits = 10;
				break;
			fmt(E5B9G9R9_UFLOAT_PACK32)
				info.rBits = 9;
				info.gBits = 9;
				info.bBits = 9;
				info.eBits = 5;
				break;
			fmt(D16_UNORM)
				info.dBits = 16;
				break;
			fmt(X8_D24_UNORM_PACK32)
				info.xBits = 8;
				info.dBits = 24;
				break;
			fmt(D32_SFLOAT)
				info.dBits = 32;
				break;
			fmt(S8_UINT)
				info.sBits = 8;
				break;

		default:
			break;
		}

#define check_channel(x) if (x > 0) info.channels++; info.blockBits += x;
		check_channel(info.rBits);
		check_channel(info.gBits);
		check_channel(info.bBits);
		check_channel(info.aBits);
		check_channel(info.dBits);
		check_channel(info.sBits);
		check_channel(info.xBits);
		check_channel(info.eBits);

		//if no block Bytes, must be a compressed format.
		if (info.blockBits == 0)
		{
			switch (format)
			{
				// ETC2
				fmt(ETC2_R8G8B8A8_UNORM_BLOCK)
				fmt(ETC2_R8G8B8A8_SRGB_BLOCK)
				fmt(EAC_R11G11_UNORM_BLOCK)
				fmt(EAC_R11G11_SNORM_BLOCK)
					info.blockBits = 128;
					info.blockW = 4;
					info.blockH = 4;
					break;
				fmt(ETC2_R8G8B8A1_UNORM_BLOCK)
				fmt(ETC2_R8G8B8A1_SRGB_BLOCK)
				fmt(ETC2_R8G8B8_UNORM_BLOCK)
				fmt(ETC2_R8G8B8_SRGB_BLOCK)
				fmt(EAC_R11_UNORM_BLOCK)
				fmt(EAC_R11_SNORM_BLOCK)
					info.blockBits = 64;
					info.blockW = 4;
					info.blockH = 4;
				break;
				// BC
				fmt(BC1_RGB_UNORM_BLOCK)
				fmt(BC1_RGB_SRGB_BLOCK)
				fmt(BC1_RGBA_UNORM_BLOCK)
				fmt(BC1_RGBA_SRGB_BLOCK)
				fmt(BC4_UNORM_BLOCK)
				fmt(BC4_SNORM_BLOCK)
					info.blockBits = 64;
					info.blockW = 4;
					info.blockH = 4;
				break;
				fmt(BC2_UNORM_BLOCK)
				fmt(BC2_SRGB_BLOCK)
				fmt(BC3_UNORM_BLOCK)
				fmt(BC3_SRGB_BLOCK)
				fmt(BC5_UNORM_BLOCK)
				fmt(BC5_SNORM_BLOCK)
				fmt(BC6H_UFLOAT_BLOCK)
				fmt(BC6H_SFLOAT_BLOCK)
				fmt(BC7_SRGB_BLOCK)
				fmt(BC7_UNORM_BLOCK)
					info.blockBits = 128;
					info.blockW = 4;
					info.blockH = 4;
				break;
			default:
				Logger::Error("Unsupport format:%d", format);
				return FormatInfo();
			}
		}
		else
		{
			info.blockW = 1;
			info.blockH = 1;
		}
#undef fmt_rgba

		return info;
	}

	void TextureFormatLayout::SetTexture1D(VkFormat format_, U32 width_, U32 arrayLayers_, U32 mipLevels_)
	{
		format = format_;
		arrayLayers = arrayLayers_;
		mipLevels = mipLevels_;
		FillMipinfo(width_, 1, 1);
	}

	void TextureFormatLayout::SetTexture2D(VkFormat format_, U32 width_, U32 height_, U32 arrayLayers_, U32 mipLevels_)
	{
		format = format_;
		arrayLayers = arrayLayers_;
		mipLevels = mipLevels_;
		FillMipinfo(width_, height_, 1);
	}

	void TextureFormatLayout::SetTexture3D(VkFormat format_, U32 width_, U32 height_, U32 depth_, U32 arrayLayers_, U32 mipLevels_)
	{
		format = format_;
		arrayLayers = arrayLayers_;
		mipLevels = mipLevels_;
		FillMipinfo(width_, height_, depth_);
	}

	void TextureFormatLayout::SetBuffer(void* buffer_, size_t size)
	{
		buffer = static_cast<uint8_t*>(buffer_);
		bufferSize = size;
	}

	U32 TextureFormatLayout::RowByteStride(U32 rowLength) const
	{
		return ((rowLength + formatInfo.blockW - 1) / formatInfo.blockW) * blockStride;
	}

	U32 TextureFormatLayout::LayerByteStride(U32 imageHeight, size_t rowByteStride) const
	{
		return ((imageHeight + formatInfo.blockH - 1) / formatInfo.blockH) * rowByteStride;
	}

	void TextureFormatLayout::FillMipinfo(U32 width, U32 height, U32 depth)
	{
		U32 offset = 0;
		formatInfo = GetFormatInfo(format, 0);
		for (U32 mip = 0; mip < mipLevels; mip++)
		{
			mipInfos[mip] = MipInfo();
			mipInfos[mip].width = width;
			mipInfos[mip].height = height;
			mipInfos[mip].depth = depth;
			mipInfos[mip].blockW = (width + formatInfo.blockW - 1) / formatInfo.blockW;
			mipInfos[mip].blockH = (height + formatInfo.blockH - 1) / formatInfo.blockH;
			mipInfos[mip].rowLength = mipInfos[mip].blockW * formatInfo.blockW;
			mipInfos[mip].imageHeight = mipInfos[mip].blockH * formatInfo.blockH;
			mipInfos[mip].offset = offset;

			offset += mipInfos[mip].blockW * mipInfos[mip].blockH * arrayLayers * depth * (formatInfo.blockBits >> 3);

			width = std::max((width >> 1u), 1u);
			height = std::max((height >> 1u), 1u);
			depth = std::max((depth >> 1u), 1u);
		}
		requiredSize = offset;
		blockStride = formatInfo.blockBits >> 3;
	}

	void TextureFormatLayout::BuildBufferImageCopies(U32& num, std::array<VkBufferImageCopy, 32>& copies) const
	{
		num = mipLevels;
		for (unsigned level = 0; level < mipLevels; level++)
		{
			const auto& mipInfo = mipInfos[level];
			VkBufferImageCopy& blit = copies[level];
			blit = {};
			blit.bufferOffset = mipInfo.offset;
			blit.bufferRowLength = mipInfo.rowLength;
			blit.bufferImageHeight = mipInfo.imageHeight;
			blit.imageSubresource.aspectMask = formatToAspectMask(format);
			blit.imageSubresource.mipLevel = level;
			blit.imageSubresource.baseArrayLayer = 0;
			blit.imageSubresource.layerCount = arrayLayers;
			blit.imageExtent.width = mipInfo.width;
			blit.imageExtent.height = mipInfo.height;
			blit.imageExtent.depth = mipInfo.depth;
		}
	}
}
}