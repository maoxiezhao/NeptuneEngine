#include "fontAltasTexture.h"
#include "renderer\renderer.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>

namespace VulkanTest
{
	struct RowData
	{
		const U8* SrcData;
		U8* DstData;
		U32 SrcRow;
		U32 DstRow;
		U32 RowWidth;
		U32 SrcTextureWidth;
		U32 DstTextureWidth;
		U32 bytesPerPixel;

		void ZeroRow()
		{
			U8* mem = &DstData[DstRow * DstTextureWidth * bytesPerPixel];
			memset(mem, 0, RowWidth * bytesPerPixel);
		}

		void CopyRow(U32 padding)
		{
			const U8* srcMem = &SrcData[SrcRow * SrcTextureWidth * bytesPerPixel];
			U8* dstMem = &DstData[(DstRow * DstTextureWidth + padding) * bytesPerPixel];
			memcpy(dstMem, srcMem, SrcTextureWidth * bytesPerPixel);

			if (padding > 0)
			{
				U8* leftPaddingPixel = &DstData[DstRow * DstTextureWidth * bytesPerPixel];
				U8* rightPaddingPixel = leftPaddingPixel + (RowWidth - 1) * bytesPerPixel;
				memset(leftPaddingPixel, 0, bytesPerPixel);
				memset(rightPaddingPixel, 0, bytesPerPixel);
			}
		}
	};

	FontAltasTexture::FontAltasTexture(VkFormat format_, PaddingStyle paddingStyle_, I32 index_, ResourceManager& resManager) :
		Texture(ResourceInfo::Temporary(Texture::ResType), resManager),
		format(format_),
		paddingStyle(paddingStyle_),
		index(index_),
		packRoot(nullptr)
	{
		auto formatInfo = GPU::GetFormatInfo(format);
		bytesPerPixel = formatInfo.GetFormatStride();
	}

	FontAltasTexture::~FontAltasTexture()
	{
	}

	void FontAltasTexture::Init(U32 width_, U32 height_)
	{
		ASSERT(packRoot == nullptr);

		U32 padding = GetPadding();
		width = width_;
		height = height_;
		packRoot = CJING_NEW(Slot)(padding, padding, width - padding, height - padding);

		texData.resize(width * height * bytesPerPixel);
		memset(texData.data(), 0, texData.size());
		isLoaded = true;
	}

	FontAltasTexture::Slot* FontAltasTexture::AddCharacter(U32 w, U32 h, const Array<U8>& data)
	{
		if (w == 0 || h == 0)
			return nullptr;

		Slot* ret = nullptr;
		U32 padding = GetPadding();
		ret = packRoot->Insert(w, h, padding * 2);
		if (ret != nullptr)
		{
			WriteDataIntoSlot(ret, data);
			isDirty = true;
		}

		return ret;
	}

	void FontAltasTexture::EnsureTextureCreated()
	{
		if (!handle)
		{
			if (!Create(width, height, format, nullptr))
				Logger::Warning("Failed to initialize font altas texture.");

			auto device = Renderer::GetDevice();
			ASSERT(device != nullptr);
			device->SetName(*handle, "FontAltas");
		}
	}

	void FontAltasTexture::Flush()
	{
		if (isDirty == false)
			return;

		EnsureTextureCreated();
		InputMemoryStream mem(texData.data(), texData.size());
		UpdateTexture(mem);
		isDirty = false;
	}

	U32 FontAltasTexture::GetPadding()const
	{
		return paddingStyle == NoPadding ? 0 : 1;
	}

	void FontAltasTexture::WriteDataIntoSlot(const Slot* slot, const Array<U8>& data)
	{
		U8* mem = &texData[slot->y * width * bytesPerPixel + slot->x * bytesPerPixel];
		const U32 padding = GetPadding();
		const U32 allPadding = padding * 2;

		RowData rowData;
		rowData.DstData = mem;
		rowData.SrcData = data.data();
		rowData.DstTextureWidth = width;
		rowData.SrcTextureWidth = slot->w - allPadding;
		rowData.RowWidth = slot->w;
		rowData.bytesPerPixel = bytesPerPixel;

		if (padding > 0)
		{
			rowData.SrcRow = 0;
			rowData.DstRow = 0;
			rowData.ZeroRow();
		}

		for (U32 row = padding; row < slot->h - padding; row++)
		{
			rowData.SrcRow = row - padding;
			rowData.DstRow = row;
			rowData.CopyRow(padding);
		}

		if (padding > 0)
		{
			rowData.SrcRow = (slot->h - allPadding) - 1;
			rowData.DstRow = slot->h - padding;
			rowData.ZeroRow();
		}
	}

	void FontAltasTexture::Unload()
	{
		Texture::Unload();
		CJING_SAFE_DELETE(packRoot);
		texData.clear();
	}
}