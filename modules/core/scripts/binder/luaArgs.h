#pragma once

#include"core\scripts\luaUtils.h"

namespace VulkanTest
{
	struct _lua_args {};

	#define LUA_ARGS_TYPE(...) _lua_args(*)(__VA_ARGS__) 
	#define LUA_ARGS(...) static_cast<LUA_ARGS_TYPE(__VA_ARGS__)>(nullptr)

	template<typename T>
	struct LuaArgHolder
	{
		T& Get() { return value; }
		const T& Get()const { return value; }
		void Set(const T& value_) { value = value_; }
		T value;
	};

	template<typename T>
	struct LuaArgHolder<T&>
	{
		T& Get() { return *value; }
		const T& Get()const { return *value; }
		void Set(T& value_) { value = &value_; }
		T* value;
	};

	template<typename T>
	struct LuaArgTraits
	{
		using ArgType = typename std::result_of<decltype(&LuaUtils::LuaType<T>::Get)(lua_State*, int)>::type;
		using ArgHolderType = LuaArgHolder<ArgType>;
	};

	template<typename T>
	struct LuaArgGetter
	{
		using ArgHolderType = typename LuaArgTraits<T>::ArgHolderType;
		using ArgType = typename LuaArgTraits<T>::ArgType;

		static int Get(lua_State* l, int index, ArgHolderType& holder)
		{
			holder.Set(LuaUtils::Get<ArgType>(l, index));
			return 1;
		}
	};

	template<typename T>
	struct LuaArg
	{
		using LuaArgHolderType = typename LuaArgTraits<T>::ArgHolderType;

		static int Get(lua_State* l, int index, LuaArgHolderType& holder)
		{
			return LuaArgGetter<T>::Get(l, index, holder);
		}
	};

	template<typename... Args>
	struct LuaArgTupleVisitor;

	template<>
	struct LuaArgTupleVisitor<>
	{
		template<typename... T>
		static void Get(lua_State* l, int index, std::tuple<T...>& tuple) {}
	};

	template<typename P, typename... Args>
	struct LuaArgTupleVisitor<P, Args... >
	{
		template<typename... T>
		static void Get(lua_State* l, int index, std::tuple<T...>& tuple)
		{
			auto& holder = std::get<sizeof...(T) - sizeof...(Args) - 1>(tuple);
			index += LuaArg<P>::Get(l, index, holder);
			return LuaArgTupleVisitor<Args...>::Get(l, index, tuple);
		}
	};

	template<typename... Args>
	struct LuaArgTuple
	{
		using Tuple = std::tuple<typename LuaArg<Args>::LuaArgHolderType...>;
		Tuple tuple;

		void Get(lua_State* l, int index)
		{
			LuaArgTupleVisitor<Args...>::Get(l, index, tuple);
		}
	};
}