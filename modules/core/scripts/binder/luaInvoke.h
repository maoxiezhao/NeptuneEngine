#pragma once

#include "luaArgs.h"

namespace VulkanTest::LuaUtils
{
namespace Binder::Invoke
{
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
}
}