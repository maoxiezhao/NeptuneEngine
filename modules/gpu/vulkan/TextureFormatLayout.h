#pragma once

#include "definition.h"

namespace VulkanTest
{
namespace GPU
{
	class DeviceVulkan;
	
	struct FormatInfo
	{
		/// Block width.
		U32 blockW = 0;
		/// Block height.
		U32 blockH = 0;
		/// Number of bits in block.
		U32 blockBits = 0;
		/// Number of bits for red channel.
		U32 rBits = 0;
		/// Number of bits for green channel.
		U32 gBits = 0;
		/// Number of bits for blue channel.
		U32 bBits = 0;
		/// Number of bits for alpha channel.
		U32 aBits = 0;
		/// Number of bits for depth.
		U32 dBits = 0;
		/// Number of bits for stencil.
		U32 sBits = 0;
		/// Number of padding bits.
		U32 xBits = 0;
		/// Number of exponent bits.
		U32 eBits = 0;
		/// Number of channels.
		U32 channels = 0;
	};

	class TextureFormatLayout
	{
	public:
		void SetTexture1D(VkFormat format_, U32 width_,  U32 arrayLayers_ = 1, U32 mipLevels = 1);
		void SetTexture2D(VkFormat format_, U32 width_, U32 height_, U32 arrayLayers_ = 1, U32 mipLevels = 1);
		void SetTexture3D(VkFormat format_, U32 width_, U32 height_, U32 depth_, U32 arrayLayers_ = 1, U32 mipLevels = 1);
		void SetBuffer(void* buffer_, size_t size);

		U32 GetRequiredSize()const { return requiredSize; }
		U32 GetBlockStride()const { return blockStride; }
		const FormatInfo& GetFormatInfo()const { return formatInfo; }

		struct MipInfo
		{
			U32 width = 1;
			U32 height = 1;
			U32 depth = 1;
			U32 blockW = 0;
			U32 blockH = 0;
			U32 rowLength = 0;
			U32 imageHeight = 0;
			U32 offset = 0;
		};
		const MipInfo& GetMipInfo(U32 level)const
		{
			return mipInfos[level];
		}

		U32 RowByteStride(U32 rowLength)const;
		U32 LayerByteStride(U32 imageHeight, size_t rowByteStride)const;
		void BuildBufferImageCopies(U32& num, std::array<VkBufferImageCopy, 32>& copies) const;

		void* Data(U32 layer = 0, U32 mip = 0)const
		{
			assert(buffer != nullptr);
			U8* slice = buffer + mipInfos[mip].offset;
			slice += blockStride * layer * mipInfos[mip].blockW * mipInfos[mip].blockH;
			return slice;
		}

	private:
		void FillMipinfo(U32 width, U32 height, U32 depth);
		FormatInfo GetFormatInfo(VkFormat format, VkImageAspectFlags aspect)const;

	private:
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageType imageType = VK_IMAGE_TYPE_MAX_ENUM;
		U32 arrayLayers = 0;
		U32 mipLevels = 0;
		MipInfo mipInfos[16];
		U32 requiredSize = 0;
		U32 blockStride = 0;
		FormatInfo formatInfo = {};
		U8* buffer = nullptr;
		size_t bufferSize = 0;
	};
}
}