#pragma once

#include "serializationFwd.h"
#include "core\utils\string.h"

namespace VulkanTest
{
namespace Serialization
{
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
	static bool ShouldSerialize(const T& v, const void* obj)
	{
		return SerializeType<T>::ShouldSerialize(v, obj);
	}
}
}