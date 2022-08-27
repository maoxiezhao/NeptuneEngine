#pragma once

#include "fontResource.h"

namespace VulkanTest
{
	// Font glyph metrics:
	//
	//                       xmin                     xmax
	//                        |                         |
	//                        |<-------- width -------->|
	//                        |                         |
	//              |         +-------------------------+----------------- ymax
	//              |         |    ggggggggg   ggggg    |     ^        ^
	//              |         |   g:::::::::ggg::::g    |     |        |
	//              |         |  g:::::::::::::::::g    |     |        |
	//              |         | g::::::ggggg::::::gg    |     |        |
	//              |         | g:::::g     g:::::g     |     |        |
	//    offsetX  -|-------->| g:::::g     g:::::g     |  offsetY     |
	//              |         | g:::::g     g:::::g     |     |        |
	//              |         | g::::::g    g:::::g     |     |        |
	//              |         | g:::::::ggggg:::::g     |     |        |
	//              |         |  g::::::::::::::::g     |     |      height
	//              |         |   gg::::::::::::::g     |     |        |
	//  baseline ---*---------|---- gggggggg::::::g-----*--------      |
	//            / |         |             g:::::g     |              |
	//     origin   |         | gggggg      g:::::g     |              |
	//              |         | g:::::gg   gg:::::g     |              |
	//              |         |  g::::::ggg:::::::g     |              |
	//              |         |   gg:::::::::::::g      |              |
	//              |         |     ggg::::::ggg        |              |
	//              |         |         gggggg          |              v
	//              |         +-------------------------+----------------- ymin
	//              |                                   |
	//              |------------- advanceX ----------->|

	struct FontCharacterInfo
	{
		I32 character = 0;
		bool isValid = false;
		I16 offsetX;
		I16 offsetY;
		I16 advanceX;
		I16 height;
		F32x2 uv;
		F32x2 uvSize;
		I8 textureIndex;
	};

	template<typename R, typename T>
	inline R Convert26Dot6ToRoundedPixel(T value)
	{
		return static_cast<R>(std::floor(value / 64.0f));
	}

	template<typename R, typename T>
	inline R ConvertPixelTo26Dot6(T value)
	{
		return static_cast<R>(value * 64);
	}

	class VULKAN_TEST_API Font : public Object
	{
	public:
		Font(FontResource* res_, I32 size_);
		~Font();

		void FlushFaceSize() const;

		FontResource* GetResource() {
			return res;
		}

		I32 GetSize()const {
			return size;
		}

		I32 GetHeight()const {
			return height;
		}

		I16 GetDescender()const {
			return descender;
		}

		void GetCharacter(I32 c, FontCharacterInfo& info);
		I32 GetKerning(I32 first, I32 second) const;

		static F32 FontScale;

	private:
		friend class FontResource;

		I32 size;
		I32 height;
		I16 descender;
		I16 ascender;
		FontResource* res;

		bool hasKerning;
		HashMap<I32, FontCharacterInfo> characterInfos;
		mutable HashMap<U32, I32> kerningTable;
	};
}