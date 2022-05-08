#pragma once

#include "core\common.h"
#include "core\memory\allocator.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
}

namespace VulkanTest::Lua
{
	void Initialize(IAllocator& allocator);
	void Uninitialize();

	lua_State* GetLuaState();
}