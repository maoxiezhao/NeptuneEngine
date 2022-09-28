#pragma once

#include "serializationFwd.h"
#include "core\utils\string.h"
#include "core\scene\world.h"
#include "math\color.h"
#include "math\geometry.h"

namespace VulkanTest
{
namespace Serialization
{
#define DESERIALIZE_HELPER(stream, name, var, defaultValue) \
    { \
        const auto m = SERIALIZE_FIND_MEMBER(stream, name); \
        if (m != stream.MemberEnd()) \
            Serialization::Deserialize(m->value, var); \
        else \
            var = defaultValue;\
    }

	template<typename T, typename ENABLED = void>
	struct SerializeTypeNormalMapping;

	template<typename T, bool IsRef>
	struct SerializeTypeClassMapping;

	struct SerializeTypeExists
	{
		template<typename T, typename = decltype(T())>
		static std::true_type test(int);

		template<typename>
		static std::false_type test(...);
	};

	template<typename T>
	struct SerializeTypeMappingExists
	{
		using type = decltype(SerializeTypeExists::test<SerializeTypeNormalMapping<T>>(0));
		static constexpr bool value = type::value;
	};

	template<typename T>
	struct SerializeType :
		std::conditional<
		std::is_class<typename std::decay<T>::type>::value &&
		!(SerializeTypeMappingExists<typename std::decay<T>::type>::value),
		SerializeTypeClassMapping<typename std::decay<T>::type, std::is_reference<T>::value>,
		SerializeTypeNormalMapping<typename std::decay<T>::type>>::type
	{};

	template<typename T>
	static void Serialize(ISerializable::SerializeStream& stream, const T& v, const void* otherObj)
	{
		SerializeType<T>::Serialize(stream, v, otherObj);
	}

	template<typename T>
	static void Deserialize(ISerializable::DeserializeStream& stream, T& v)
	{
		SerializeType<T>::Deserialize(stream, v);
	}

	template<typename T>
	static void Deserialize(ISerializable::DeserializeStream& stream, T& v, World* world)
	{
		SerializeType<T>::Deserialize(stream, v, world);
	}

	template<typename T>
	static bool ShouldSerialize(const T& v, const void* obj)
	{
		return SerializeType<T>::ShouldSerialize(v, obj);
	}

	inline I32 DeserializeInt(ISerializable::DeserializeStream& stream)
	{
		I32 result = 0;
		if (stream.IsInt())
			result = stream.GetInt();
		else if (stream.IsFloat())
			result = (I32)stream.GetFloat();
		else if (stream.IsString())
			FromCString(Span(stream.GetString(), StringLength(stream.GetString())), result);
		return result;
	}

	Guid DeserializeGuid(ISerializable::DeserializeStream& value);
	void SerializeEntity(ISerializable::SerializeStream& stream, ECS::Entity entity);
	ECS::Entity DeserializeEntity(ISerializable::DeserializeStream& stream, World* world);

	template<typename T>
	struct SerializeTypeBase
	{
		static bool ShouldSerialize(const T& v, const void* obj)
		{
			return !obj || v != *(T*)obj;
		}
	};

	template<>
	struct SerializeTypeNormalMapping<I32> : SerializeTypeBase<I32>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const I32& v, const void* otherObj)
		{
			stream.Int(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, I32& v)
		{
			v = DeserializeInt(stream);
		}
	};

	template<>
	struct SerializeTypeNormalMapping<I64> : SerializeTypeBase<I64>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const I64& v, const void* otherObj)
		{
			stream.Int64(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, I64& v)
		{
			v = stream.GetInt64();
		}
	};

	template<>
	struct SerializeTypeNormalMapping<F32> : SerializeTypeBase<F32>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const F32& v, const void* otherObj)
		{
			stream.Float(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, F32& v)
		{
			v = stream.GetFloat();
		}
	};

	template<>
	struct SerializeTypeNormalMapping<U8> : SerializeTypeBase<U8>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const U8& v, const void* otherObj)
		{
			stream.Uint((U32)v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, U8& v)
		{
			v = (U8)stream.GetUint();
		}
	};

	template<>
	struct SerializeTypeNormalMapping<U16> : SerializeTypeBase<U16>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const U16& v, const void* otherObj)
		{
			stream.Uint((U32)v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, U16& v)
		{
			v = (U16)stream.GetUint();
		}
	};

	template<>
	struct SerializeTypeNormalMapping<U32> : SerializeTypeBase<U32>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const U32& v, const void* otherObj)
		{
			stream.Uint(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, U32& v)
		{
			v = stream.GetUint();
		}
	};

	template<>
	struct SerializeTypeNormalMapping<F64> : SerializeTypeBase<F64>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const F64& v, const void* otherObj)
		{
			stream.Double(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, F64& v)
		{
			v = stream.GetDouble();
		}
	};

	template<>
	struct SerializeTypeNormalMapping<String> : SerializeTypeBase<String>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const String& v, const void* otherObj)
		{
			stream.String(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, String& v)
		{
			v = stream.GetString();
		}
	};

	template<>
	struct SerializeTypeNormalMapping<Guid> : SerializeTypeBase<Guid>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const Guid& v, const void* otherObj)
		{
			stream.Guid(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, Guid& v)
		{
			v = DeserializeGuid(stream);
		}

		static bool ShouldSerialize(const Guid& v, const void* obj)
		{
			return v.IsValid();
		}
	};

	template<typename T>
	struct SerializeTypeNormalMapping<T, typename std::enable_if<std::is_enum<T>::value, void>::type> : SerializeTypeBase<T>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const T& v, const void* otherObj)
		{
			stream.Uint((U32)v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, T& v)
		{
			v = (T)DeserializeInt(stream);
		}
	};

	template<>
	struct SerializeTypeNormalMapping<F32x2>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const F32x2& v, const void* otherObj)
		{
			stream.Float2(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, F32x2& v)
		{
			const auto x = SERIALIZE_FIND_MEMBER(stream, "X");
			const auto y = SERIALIZE_FIND_MEMBER(stream, "Y");
			v.x = x != stream.MemberEnd() ? x->value.GetFloat() : 0.0f;
			v.y = y != stream.MemberEnd() ? y->value.GetFloat() : 0.0f;
		}

		static bool ShouldSerialize(const F32x2& v, const void* obj)
		{
			return !obj;
		}
	};

	template<>
	struct SerializeTypeNormalMapping<F32x3>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const F32x3& v, const void* otherObj)
		{
			stream.Float3(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, F32x3& v)
		{
			const auto x = SERIALIZE_FIND_MEMBER(stream, "X");
			const auto y = SERIALIZE_FIND_MEMBER(stream, "Y");
			const auto z = SERIALIZE_FIND_MEMBER(stream, "Z");
			v.x = x != stream.MemberEnd() ? x->value.GetFloat() : 0.0f;
			v.y = y != stream.MemberEnd() ? y->value.GetFloat() : 0.0f;
			v.z = z != stream.MemberEnd() ? z->value.GetFloat() : 0.0f;
		}

		static bool ShouldSerialize(const F32x3& v, const void* obj)
		{
			return !obj;
		}
	};

	template<>
	struct SerializeTypeNormalMapping<F32x4> : SerializeTypeBase<F32x4>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const F32x4& v, const void* otherObj)
		{
			stream.Float4(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, F32x4& v)
		{
			const auto x = SERIALIZE_FIND_MEMBER(stream, "X");
			const auto y = SERIALIZE_FIND_MEMBER(stream, "Y");
			const auto z = SERIALIZE_FIND_MEMBER(stream, "Z");
			const auto w = SERIALIZE_FIND_MEMBER(stream, "W");
			v.x = x != stream.MemberEnd() ? x->value.GetFloat() : 0.0f;
			v.y = y != stream.MemberEnd() ? y->value.GetFloat() : 0.0f;
			v.z = z != stream.MemberEnd() ? z->value.GetFloat() : 0.0f;
			v.w = w != stream.MemberEnd() ? w->value.GetFloat() : 0.0f;
		}

		static bool ShouldSerialize(const F32x4& v, const void* obj)
		{
			return !obj;
		}
	};

	template<>
	struct SerializeTypeNormalMapping<AABB>
	{
		static void Serialize(ISerializable::SerializeStream& stream, const AABB& v, const void* otherObj)
		{
			stream.WriteAABB(v);
		}

		static void Deserialize(ISerializable::DeserializeStream& stream, AABB& v)
		{
			DESERIALIZE_HELPER(stream, "Min", v.min, F32x3(0.0f));
			DESERIALIZE_HELPER(stream, "Max", v.max, F32x3(0.0f));
		}

		static bool ShouldSerialize(const AABB& v, const void* obj)
		{
			return !obj;
		}
	};
}
}