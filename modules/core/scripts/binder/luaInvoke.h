#pragma once

#include "luaArgs.h"

namespace VulkanTest::LuaUtils
{
namespace Binder::Invoke
{
	template<typename>
	struct function_helper;

	template<typename Ret, typename... Args>
	struct function_helper<Ret(Args...)> 
	{
		using return_type = std::remove_cv_t<Ret>;
		using args_type = std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>;

		static constexpr auto size = sizeof...(Args);
		static constexpr auto is_const = false;

		static auto args(lua_State* l, int index)
		{
			LuaArgTuple<Args...> args;
			args.Get(l, index);
			return args;
		}
	};

	template<typename Ret, typename... Args>
	struct function_helper<Ret(Args...) const> : function_helper<Ret(Args...)> {
		static constexpr auto is_const = true;
	};

	template<typename Ret, typename... Args, typename Class>
	constexpr function_helper<Ret(Args...)>
		to_function_helper(Ret(Class::*)(Args...));


	template<typename Ret, typename... Args, typename Class>
	constexpr function_helper<Ret(Args...) const>
		to_function_helper(Ret(Class::*)(Args...) const);


	template<typename Ret, typename... Args>
	constexpr function_helper<Ret(Args...)>
		to_function_helper(Ret(*)(Args...));

	template<typename Candidate>
	using function_helper_t = decltype(to_function_helper(std::declval<Candidate>()));

	// To get a index sequence (0, 1, ...N)
	template<typename T, typename Tuple, size_t N, size_t... Index>
	struct ClassConstructorDispatch : ClassConstructorDispatch<T, Tuple, N - 1, N - 1, Index...> {};

	template<typename T, typename Tuple, size_t... Index>
	struct ClassConstructorDispatch<T, Tuple, 0, Index...>
	{
		static void Call(void* mem, Tuple& args)
		{
			new(mem)T(std::get<Index>(args).Get()...);
		}
	};

	template<typename T>
	struct ClassConstructor
	{
		template<typename... Args>
		static void Call(void* mem, std::tuple<Args...>& args)
		{
			ClassConstructorDispatch<T, std::tuple<Args...>, sizeof...(Args)>::Call(mem, args);
		}
	};

	template<typename T, auto Func, typename R, typename Tuple, size_t N, size_t... Index>
	struct ClassMethodDispatch : ClassMethodDispatch<T, Func, R, Tuple, N - 1, N - 1, Index...> {};

	template<typename T, auto Func, typename R, typename Tuple, size_t... Index>
	struct ClassMethodDispatch<T, Func, R, Tuple, 0, Index...>
	{
		static R Call(T* obj, Tuple& args)
		{
			return std::invoke(Func, obj, std::get<Index>(args).Get()...);
		}
	};

	template<typename T, auto Func>
	struct ClassMethodCaller
	{
		using FunctionHelper = function_helper_t<decltype(Func)>;
		using ReturnType = typename FunctionHelper::return_type;

		template<typename... Args>
		static int Call(lua_State* l, T* obj, std::tuple<Args...> args)
		{		
			if constexpr (std::is_void_v<ReturnType>)
			{
				ClassMethodDispatch<T, Func, ReturnType, std::tuple<Args...>, sizeof...(Args)>::Call(obj, args);
				return 0;
			}
			else
			{
				ReturnType ret = ClassMethodDispatch<T, Func, ReturnType, std::tuple<Args...>, sizeof...(Args)>::Call(obj, args);
				Push<ReturnType>(l, ret);
				return 1;
			}
		}
	};

	template<auto Func, typename R, typename Tuple, size_t N, size_t... Index>
	struct ClassFunctionDispatch : ClassFunctionDispatch<Func, R, Tuple, N - 1, N - 1, Index...> {};

	template<auto Func, typename R, typename Tuple, size_t... Index>
	struct ClassFunctionDispatch<Func, R, Tuple, 0, Index...>
	{
		static R Call(Tuple& args)
		{
			return std::invoke(Func, std::get<Index>(args).Get()...);
		}
	};

	template<auto Func>
	struct StaticMethodCaller
	{
		using FunctionHelper = function_helper_t<decltype(Func)>;
		using ReturnType = typename FunctionHelper::return_type;

		template<typename... Args>
		static int Call(lua_State* l, std::tuple<Args...> args)
		{
			if constexpr (std::is_void_v<ReturnType>)
			{
				ClassFunctionDispatch<Func, ReturnType, std::tuple<Args...>, sizeof...(Args)>::Call(args);
				return 0;
			}
			else
			{
				ReturnType ret = ClassFunctionDispatch<Func, ReturnType, std::tuple<Args...>, sizeof...(Args)>::Call(args);
				Push<ReturnType>(l, ret);
				return 1;
			}
		}
	};
}
}