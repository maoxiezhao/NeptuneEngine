#include "lua.h"

namespace VulkanTest::Lua
{
	struct LuaInternal
	{
		lua_State* luaState = NULL;
		size_t allocated = 0;
		IAllocator* allocator = nullptr;
	};
	LuaInternal impl;

	void Initialize(IAllocator& allocator)
	{
		impl.luaState = luaL_newstate();
		impl.allocator = &allocator;
		luaL_openlibs(impl.luaState);
	}

	void Uninitialize()
	{
		ASSERT(impl.luaState != nullptr);
		lua_close(impl.luaState);
	}

	lua_State* GetLuaState()
	{
		return impl.luaState;
	}
}