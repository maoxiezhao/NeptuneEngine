#pragma once

#include "math_common.h"

namespace VulkanTest
{
#ifdef _MSC_VER
#pragma warning(disable: 4307)
#endif

#define FNV_64_PRIME (0x100000001b3ULL)

	constexpr U64 FnvIterate(U64 hash, U64 c)
	{
		return (hash * FNV_64_PRIME) ^ c;
	}

	template<size_t index>
	constexpr U64 CompileTimeFnv1Inner(U64 hash, const char* str)
	{
		return CompileTimeFnv1Inner<index - 1>(FnvIterate(hash, U8(str[index])), str);
	}

	template<>
	constexpr U64 CompileTimeFnv1Inner<size_t(-1)>(U64 hash, const char*)
	{
		return hash;
	}

	template<size_t len>
	constexpr U64 CompileTimeFnv1(const char(&str)[len])
	{
		return CompileTimeFnv1Inner<len - 1>(0xcbf29ce484222325ull, str);
	}
}