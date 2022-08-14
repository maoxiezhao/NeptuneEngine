#include "luaObject.h"

namespace VulkanTest::LuaUtils::Binder
{
	LuaObject* LuaObject::GetLuaObject(lua_State* l, int index, void* classID)
	{
		index = GetPositiveIndex(l, index);
		if (!lua_isuserdata(l, index))
		{
			LuaException::Error(l, "Except userdata, got %s.", lua_typename(l, lua_type(l, index)));
			return nullptr;
		}

		// Get class metatable from LUA_REGISTRYINDEX by classID
		lua_rawgetp(l, LUA_REGISTRYINDEX, classID);
		if (!lua_istable(l, -1))
		{
			LuaException::ArgError(l, -1, "Invalid binding lua class");
			return nullptr;
		}

		// Get class metatable from the current index
		lua_getmetatable(l, index);
		if (!lua_istable(l, -1))
		{
			LuaException::ArgError(l, index, "Invalid binding lua class");
			return nullptr;
		}

		// Check metatables is matched
		bool matched = false;
		while (!lua_isnil(l, -1))
		{
			if (lua_rawequal(l, -1, -2))
			{
				matched = true;
				break;
			}

			lua_pushstring(l, "__SUPER");
			lua_rawget(l, -2);

			// stack: expected_meta meta super_meta
			lua_remove(l, -2);
			// stack: expected_meta super_meta
		}
		lua_pop(l, 2);

		return matched ? static_cast<LuaObject*>(lua_touserdata(l, index)) : nullptr;
	}

	int LuaObject::MetaIndex(lua_State* l)
	{
		if (!IsValid(l, 1))
		{
			LuaException::Error(l, "Except userdata, got %s.", lua_typename(l, lua_type(l, 1)));
			return 1;
		}

		lua_getmetatable(l, 1);
		while (true)
		{
			// Find in metatable
			lua_pushvalue(l, 2);
			lua_rawget(l, -2);
			if (!lua_isnil(l, -1))
				break;
			lua_pop(l, 1);

			// Find in getters
			lua_pushliteral(l, "___getters");
			lua_rawget(l, -2);
			lua_pushvalue(l, 2);
			lua_rawget(l, -2);
		
			// stack: meta __getters getter
			if (!lua_isnil(l, -1) && lua_iscfunction(l, -1))
			{
				if (lua_isuserdata(l, 1))	// Called table is a LuaObject
				{
					lua_pushvalue(l, 1);
					lua_call(l, 1, 1);
				}
				else
				{
					lua_call(l, 0, 1);
				}
				break;
			}
			lua_pop(l, 2);	// Stack: meta

			// Find in super class metatable
			lua_pushliteral(l, "__SUPER");
			lua_rawget(l, -2);
			if (lua_isnil(l, -1))
				break;
			
			// stack: meta super_meta
			lua_remove(l, -2);
			// stack: super_meta
		}
		return 1;
	}

	int LuaObject::MetaNewIndex(lua_State* l)
	{
		if (!IsValid(l, 1))
		{
			LuaException::Error(l, "Except userdata, got %s.", lua_typename(l, lua_type(l, 1)));
			return 1;
		}

		lua_getmetatable(l, 1);
		while (true)
		{
			lua_pushliteral(l, "___setters");
			lua_rawget(l, -2);
			lua_pushvalue(l, 2);
			lua_rawget(l, -2);

			// stack:meta __setters setter
			if (!lua_isnil(l, -1) && lua_iscfunction(l, -1))
			{
				if (lua_isuserdata(l, 1))
				{
					lua_pushvalue(l, 1);
					lua_pushvalue(l, 3);
					lua_call(l, 2, 0);
				}
				else
				{
					lua_pushvalue(l, 3);
					lua_call(l, 1, 0);
				}
				break;
			}
			lua_pop(l, 2); // stack: meta


			lua_pushliteral(l, "__SUPER");
			lua_rawget(l, -2);
			if (lua_isnil(l, -1))
				break;

			// stack: meta super_meta
			lua_remove(l, -2);
			// stack: super_meta
		}

		return 0;
	}

	bool LuaObject::IsValid(lua_State* l, int index)
	{
		// 1. Use ClassIDGenerator<LuaObject>::ID() to get ClassIDRef
		// 2. Get ClassMetatable from REGISTRYINDEX by ClassIDRef
		// 3. Check whether the two metatables are equal

		index = GetPositiveIndex(l, index);
		lua_getmetatable(l, index);
		lua_rawgetp(l, -1, ClassIDGenerator<LuaObject>::ID());
		lua_rawget(l, LUA_REGISTRYINDEX);
		bool ret = (lua_rawequal(l, -1, -2));
		lua_pop(l, 2);
		return ret;
	}
}