#pragma once

#include "luaCommon.h"

#include <exception>
#include <string>

namespace VulkanTest::LuaUtils
{
	class LuaException : public std::exception
	{
	public:
		LuaException(lua_State*l, const char* msg) : l(l), errMsg(msg) {};
		lua_State* GetLuaState()const { return l; }
		virtual const char* what()const throw() override { return errMsg.c_str(); }

		static void Error(lua_State * l, const char* format, ...);
		static void ArgError(lua_State * l, int index, const char* format, ...);
	private:
		String errMsg;
		lua_State* l;
	};
}