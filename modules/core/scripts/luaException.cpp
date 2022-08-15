#include "luaException.h"

#include <iostream>
#include <sstream>

namespace VulkanTest::LuaUtils
{
	void LuaException::Error(lua_State* l, const char* format, ...)
	{
		StaticString<64> msg;
		va_list args;
		va_start(args, format);
		vsnprintf(msg.data, 64, format, args);
		va_end(args);
		luaL_traceback(l, l, NULL, 1);
		msg += lua_tostring(l, -1);
		throw LuaException(l, msg);
	}

	void LuaException::ArgError(lua_State* l, int index, const char* format, ...)
	{
		StaticString<64> msg;
		va_list args;
		va_start(args, format);
		vsnprintf(msg.data, 64, format, args);
		va_end(args);

		std::ostringstream oss;
		lua_Debug info;
		if (!lua_getstack(l, 0, &info))
		{
			oss << "Bad argument #" << index << "(" << msg.c_str() << ")";
			Error(l, oss.str().c_str());
		}

		lua_getinfo(l, "n", &info);
		if (String(info.namewhat) == "method")
		{
			oss << "Calling:" << info.name << " failed. (" << msg.c_str() << ")";
			Error(l, oss.str().c_str());
		}

		if (info.name == nullptr)
			info.name = "?";

		oss << "Bad argument #" << index << " to " << info.name << "(" << msg.c_str() << ")";
		Error(l, oss.str().c_str());
	}
}