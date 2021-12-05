#pragma once

#include "vMath.h"

namespace VulkanTest {

	class Color4
	{
	public:
		U32 rgba = 0;

		constexpr Color4(U32 color) : rgba(color) {};
		constexpr Color4(U8 r = 0, U8 g = 0, U8 b = 0, U8 a = 0) : rgba(r << 0 | g << 8 | b << 16 | a << 24) {};
		constexpr Color4(const Vec4& value) { Convert(value); }

		constexpr U8 GetR()const { return (rgba >> 0) & 0xFF; }
		constexpr U8 GetG()const { return (rgba >> 8) & 0xFF; }
		constexpr U8 GetB()const { return (rgba >> 16) & 0xFF; }
		constexpr U8 GetA()const { return (rgba >> 24) & 0xFF; }

		constexpr void SetR(U8 v) { *this = Color4(v, GetG(), GetB(), GetA());}
		constexpr void SetG(U8 v) { *this = Color4(GetA(), v, GetB(), GetA()); }
		constexpr void SetB(U8 v) { *this = Color4(GetA(), GetG(), v, GetA()); }
		constexpr void SetA(U8 v) { *this = Color4(GetA(), GetG(), GetB(), v); }

		constexpr void SetFloatR(F32 v) { *this = Color4(U8(v * 255), GetG(), GetB(), GetA()); }
		constexpr void SetFloatG(F32 v) { *this = Color4(GetA(), U8(v * 255), GetB(), GetA()); }
		constexpr void SetFloatB(F32 v) { *this = Color4(GetA(), GetG(), U8(v * 255), GetA()); }
		constexpr void SetFloatA(F32 v) { *this = Color4(GetA(), GetG(), GetB(), U8(v * 255)); }

		U32 GetRGBA()const { return rgba; }

		// convert to 0.0-1.0
		constexpr Vec3 ToFloat3() const
		{
			return Vec3(
				((rgba >> 0) & 0xff) / 255.0f,
				((rgba >> 8) & 0xff) / 255.0f,
				((rgba >> 16) & 0xff) / 255.0f
			);
		}

		// convert to 0.0-1.0
		constexpr Vec4 ToFloat4() const
		{
			return Vec4(
				((rgba >> 0) & 0xff) / 255.0f,
				((rgba >> 8) & 0xff) / 255.0f,
				((rgba >> 16) & 0xff) / 255.0f,
				((rgba >> 24) & 0xff) / 255.0f
			);
		}

		static constexpr Color4 Convert(const Vec3& value)
		{
			return Color4(
				(U8)(value[0] * 255.0f),
				(U8)(value[1] * 255.0f),
				(U8)(value[2] * 255.0f)
			);
		}

		static constexpr Color4 Convert(const Vec4& value)
		{
			return Color4(
				(U8)(value[0] * 255.0f),
				(U8)(value[1] * 255.0f),
				(U8)(value[2] * 255.0f),
				(U8)(value[3] * 255.0f)
			);
		}

		static constexpr Color4 Red() { return Color4(255, 0, 0, 255); }
		static constexpr Color4 Green() { return Color4(0, 255, 0, 255); }
		static constexpr Color4 Blue() { return Color4(0, 0, 255, 255); }
		static constexpr Color4 Black() { return Color4(0, 0, 0, 255); }
		static constexpr Color4 White() { return Color4(255, 255, 255, 255); }
		static constexpr Color4 Yellow() { return Color4(255, 255, 0, 255); }
		static constexpr Color4 Pink() { return Color4(255, 0, 255, 255); }

		bool operator==(const Color4& color)const { return rgba == color.rgba; }
		bool operator!=(const Color4& color)const { return rgba != color.rgba; }
	};
}