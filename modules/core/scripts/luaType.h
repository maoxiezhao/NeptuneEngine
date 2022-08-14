#pragma once

#include "luaCommon.h"
#include "luaException.h"

#include "core\utils\string.h"
#include "core\utils\path.h"
#include "math\math.hpp"

namespace VulkanTest::LuaUtils
{
	template<typename T, typename ENABLED = void>
	struct LuaTypeNormalMapping;

	template<typename T, bool IsRef>
	struct LuaTypeClassMapping;

	struct LuaTypeExists
	{
		template<typename T, typename = decltype(T())>
		static std::true_type test(int);

		template<typename>
		static std::false_type test(...);
	};

	template<typename T>
	struct LuaTypeMappingExists
	{
		using type = decltype(LuaTypeExists::test<LuaTypeNormalMapping<T>>(0));
		static constexpr bool value = type::value;
	};

	template<typename T>
	struct LuaType :
		std::conditional<
		std::is_class<typename std::decay<T>::type>::value &&
		!(LuaTypeMappingExists<typename std::decay<T>::type>::value),
		LuaTypeClassMapping<typename std::decay<T>::type, std::is_reference<T>::value>,
		LuaTypeNormalMapping<typename std::decay<T>::type>>::type
	{};

	inline int GetPositiveIndex(lua_State* l, int index)
	{
		int positiveIndex = index;
		if (index < 0 && index >= -lua_gettop(l))
			positiveIndex = lua_gettop(l) + index + 1;

		return positiveIndex;
	}

	//---------------------------------- Normal type mapping ----------------------------------------//

	template<typename T>
	struct LuaTypeNumberMapping
	{
		static void Push(lua_State* l, const T& value)
		{
			lua_pushnumber(l, static_cast<lua_Number>(value));
		}

		static T Get(lua_State* l, int index)
		{
			index = GetPositiveIndex(l, index);
			if (!lua_isnumber(l, index))
				LuaException::ArgError(l, index, "Excepted:Number, got %s.", luaL_typename(l, index));

			return static_cast<T>(lua_tonumber(l, index));
		}

		static T Opt(lua_State* l, int index, T defValue)
		{
			return static_cast<T>(luaL_optnumber(l, index, defValue));
		}

		static bool Check(lua_State* l, int index)
		{
			return lua_isnumber(l, index);
		}
	};

	template<> struct LuaTypeNormalMapping<F32> : LuaTypeNumberMapping<F32> {};
	template<> struct LuaTypeNormalMapping<F64> : LuaTypeNumberMapping<F64> {};

	template<typename T>
	struct LuaTypeIntegerMapping
	{
		static void Push(lua_State* l, const T& value)
		{
			lua_pushinteger(l, static_cast<lua_Integer>(value));
		}

		static T Get(lua_State* l, int index)
		{
			index = GetPositiveIndex(l, index);
			if (!lua_isinteger(l, index))
				LuaException::ArgError(l, index, "Excepted:Integer, got %s.", luaL_typename(l, index));

			return static_cast<T>(lua_tointeger(l, index));
		}

		static T Opt(lua_State* l, int index, T defValue)
		{
			return static_cast<T>(luaL_optinteger(l, index, defValue));
		}

		static bool Check(lua_State* l, int index)
		{
			return lua_isinteger(l, index);
		}
	};

	template<> struct LuaTypeNormalMapping<I16> : LuaTypeIntegerMapping<I16> {};
	template<> struct LuaTypeNormalMapping<I32> : LuaTypeIntegerMapping<I32> {};
	template<> struct LuaTypeNormalMapping<I64> : LuaTypeIntegerMapping<I64> {};
	template<> struct LuaTypeNormalMapping<U16> : LuaTypeIntegerMapping<U16> {};
	template<> struct LuaTypeNormalMapping<U32> : LuaTypeIntegerMapping<U32> {};
	template<> struct LuaTypeNormalMapping<U64> : LuaTypeIntegerMapping<U64> {};

	template<>
	struct LuaTypeNormalMapping<bool>
	{
		static void Push(lua_State* l, bool value)
		{
			lua_pushboolean(l, value);
		}

		static bool Get(lua_State* l, int index)
		{
			index = GetPositiveIndex(l, index);
			if (!lua_isboolean(l, index))
				LuaException::ArgError(l, index, "Excepted:boolean, got %s", luaL_typename(l, index));

			return lua_toboolean(l, index);
		}

		static bool Opt(lua_State* l, int index, bool defValue)
		{
			return lua_isnone(l, index) ? defValue : lua_toboolean(l, index);
		}

		static bool Check(lua_State* l, int index)
		{
			return lua_isboolean(l, index);
		}
	};

	template<>
	struct LuaTypeNormalMapping<char*>
	{
		static void Push(lua_State* l, const char* value)
		{
			lua_pushstring(l, value);
		}

		static const char* Get(lua_State* l, int index)
		{
			index = GetPositiveIndex(l, index);
			if (!lua_isstring(l, index))
				LuaException::ArgError(l, index, "Excepted:char*, got ", luaL_typename(l, index));

			return lua_tostring(l, index);
		}

		static const char* Opt(lua_State* l, int index, const char* defValue)
		{
			return lua_isnoneornil(l, index) ? defValue : Get(l, index);
		}

		static bool Check(lua_State* l, int index)
		{
			return lua_isstring(l, index);
		}
	};

	template<>
	struct LuaTypeNormalMapping<lua_CFunction>
	{
		static void Push(lua_State* l, lua_CFunction value)
		{
			lua_pushcfunction(l, value);
		}

		static lua_CFunction Get(lua_State* l, int index)
		{
			return lua_tocfunction(l, index);
		}

		static bool Check(lua_State* l, int index)
		{
			return lua_iscfunction(l, index);
		}
	};

	template<>
	struct LuaTypeNormalMapping<Path>
	{
		static void Push(lua_State* l, const Path& p)
		{
			lua_pushstring(l, p.c_str());
		}

		static Path Get(lua_State* l, int index)
		{
			index = GetPositiveIndex(l, index);
			if (!lua_isstring(l, index))
				LuaException::ArgError(l, index, "Excepted:path, got ", luaL_typename(l, index));

			return Path(lua_tostring(l, index));
		}

		static Path Opt(lua_State* l, int index, const Path& defValue)
		{
			return lua_isnoneornil(l, index) ? defValue : Get(l, index);
		}

		static bool Check(lua_State* l, int index)
		{
			return lua_isstring(l, index);
		}
	};

	template<>
	struct LuaTypeNormalMapping<String>
	{
		static void Push(lua_State* l, const String& str)
		{
			lua_pushstring(l, str.c_str());
		}

		static String Get(lua_State* l, int index)
		{
			index = GetPositiveIndex(l, index);
			if (!lua_isstring(l, index))
				LuaException::ArgError(l, index, "Excepted:String, got ", luaL_typename(l, index));

			return String(lua_tostring(l, index));
		}

		static String Opt(lua_State* l, int index, const String& defValue)
		{
			return lua_isnoneornil(l, index) ? defValue : Get(l, index);
		}

		static bool Check(lua_State* l, int index)
		{
			return lua_isstring(l, index);
		}
	};

	template<typename T, size_t N>
	struct LuaTypeArrayNumberMapping
	{
		static void Push(lua_State* l, const T& value)
		{
			lua_createtable(l, N, 0);
			for (int i = 1; i <= N; i++)
			{
				lua_pushnumber(l, value.data[i - 1]);
				lua_rawseti(l, -2, i);
			}
		}

		static T Get(lua_State* l, int index)
		{
			T ret = {};
			for (int i = 1; i <= N; i++)
			{
				lua_rawgeti(l, index, i);
				ret.data[i - 1] = (F32)lua_tonumber(l, -1);
				lua_pop(l, 1);
			}
			return ret;
		}

		static T Opt(lua_State* l, int index, const T& defValue)
		{
			return lua_isnoneornil(l, index) ? defValue : Get(l, index);
		}

		static bool Check(lua_State* l, int index)
		{
			if (lua_istable(l, index) == 0)
				return false;
			lua_len(l, index);
			auto dim = lua_tointeger(l, -1);
			lua_pop(l, 1);
			return dim == N;
		}
	};

	template<> struct LuaTypeNormalMapping<F32x2> : LuaTypeArrayNumberMapping<F32x2, 2> {};
	template<> struct LuaTypeNormalMapping<F32x3> : LuaTypeArrayNumberMapping<F32x3, 3> {};
	template<> struct LuaTypeNormalMapping<F32x4> : LuaTypeArrayNumberMapping<F32x4, 4> {};
}