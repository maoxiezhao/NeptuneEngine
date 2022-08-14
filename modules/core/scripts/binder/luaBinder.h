#pragma once

#include "luaBindMeta.h"

namespace VulkanTest::LuaUtils
{
	class VULKAN_TEST_API LuaBinder
	{
	public:
		LuaBinder(lua_State* l_) :
			l(l_),
			meta(LuaRef::CreateGlobalRef(l))
		{
		}

		template<typename T>
		Binder::LuaBindClassMeta<T, LuaBinder> BeginClass(const String& name)
		{
			return Binder::LuaBindClassMeta<T, LuaBinder>::BindClass(l, meta, name);
		}

	private:
		lua_State* l;
		LuaRef meta;
	};
}