#include "jsonWriter.h"

namespace VulkanTest
{
	void JsonWriterBase::Guid(const VulkanTest::Guid& guid)
	{
		// 128bits => 32 chars
		// Binary to hexadecimal
		char buffer[32] =
		{
			'0','0', '0', '0', '0', '0', '0', '0',
			'0','0', '0', '0', '0', '0', '0', '0',
			'0','0', '0', '0', '0', '0', '0', '0',
			'0','0', '0', '0', '0', '0', '0', '0'
		};
		static const char* digits = "0123456789abcdef";
		U32 n = guid.A;
		char* p = buffer + 7;
		do
		{
			*p-- = digits[n & 0xf];
		} while ((n >>= 4) != 0);
		n = guid.B;
		p = buffer + 15;
		do
		{
			*p-- = digits[n & 0xf];
		} while ((n >>= 4) != 0);
		n = guid.C;
		p = buffer + 23;
		do
		{
			*p-- = digits[n & 0xf];
		} while ((n >>= 4) != 0);
		n = guid.D;
		p = buffer + 31;
		do
		{
			*p-- = digits[n & 0xf];
		} while ((n >>= 4) != 0);
		String(buffer, 32);
	}

	void JsonWriterBase::Entity(const ECS::Entity& entity)
	{
		StartObject();

		JKEY("Path");
		auto path = entity.GetPath();
		String(path.c_str(), path.length());

		JKEY("Label");
		auto name = entity.GetName();
		String(name, StringLength(name));

		auto parent = entity.GetParent();
		if (parent != ECS::INVALID_ENTITY)
		{
			JKEY("Parent");
			auto name = parent.GetName();
			String(name, StringLength(name));
		}

		EndObject();
	}


	void JsonWriterBase::Float2(const F32x2& value)
	{
		StartObject();
		JKEY("X");
		Float(value.x);
		JKEY("Y");
		Float(value.y);
		EndObject();
	}

	void JsonWriterBase::Float3(const F32x3& value)
	{
		StartObject();
		JKEY("X");
		Float(value.x);
		JKEY("Y");
		Float(value.y);
		JKEY("Z");
		Float(value.z);
		EndObject();
	}

	void JsonWriterBase::Float4(const F32x4& value)
	{
		StartObject();
		JKEY("X");
		Float(value.x);
		JKEY("Y");
		Float(value.y);
		JKEY("Z");
		Float(value.z);
		JKEY("W");
		Float(value.w);
		EndObject();
	}

	void JsonWriterBase::WriteAABB(const AABB& aabb)
	{
		StartObject();
		JKEY("Min");
		Float3(aabb.min);
		JKEY("Max");
		Float3(aabb.max);
		EndObject();
	}
}