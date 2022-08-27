#pragma once

#include "fontResource.h"
#include "renderer\texture.h"
#include "core\utils\rectPacker.h"

namespace VulkanTest
{
	class FontAltasTexture final : public Texture
	{
	public:
		enum PaddingStyle
		{
			NoPadding,		// No padding
			PadWithZero,	// One pixel uniform padding border filled with zeros.		
		};

		struct Slot : ReckPacker<Slot>
		{
			Slot(U32 x, U32 y, U32 width, U32 height)
				: ReckPacker<Slot>(x, y, width, height)
			{
			}
		};

		FontAltasTexture(VkFormat format_, PaddingStyle paddingStyle_, I32 index_, ResourceFactory& resFactory);
		~FontAltasTexture();

		void Init(U32 width_, U32 height_);
		Slot* AddCharacter(U32 w, U32 h, const Array<U8>& data);
		void EnsureTextureCreated();
		void Flush();

		U32 GetPadding()const;

		F32x2 GetSize()const {
			return F32x2((F32)width, (F32)height);
		}

		bool IsDirty()const {
			return isDirty;
		}

	protected:
		void WriteDataIntoSlot(const Slot* slot, const Array<U8>& data);
		void Unload() override;

	private:
		U32 width = 0;
		U32 height = 0;
		VkFormat format;
		I32 index;
		PaddingStyle paddingStyle;
		U32 bytesPerPixel;
		Array<U8> texData;
		bool isDirty = false;

		Slot* packRoot;
	};
}