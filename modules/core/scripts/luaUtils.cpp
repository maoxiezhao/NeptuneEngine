#include "luaUtils.h"

namespace VulkanTest::LuaUtils
{
	LuaRef LuaRef::NULL_REF;

	LuaRef::LuaRef() :
		l(nullptr),
		ref(LUA_REFNIL)
	{
	}

	LuaRef::LuaRef(lua_State* l, int ref_) :
		l(l),
		ref(ref_)
	{
	}

	LuaRef::LuaRef(const LuaRef& other) :
		l(other.l),
		ref(LUA_REFNIL)
	{
		if (l != nullptr && !other.IsEmpty())
		{
			lua_rawgeti(l, LUA_REGISTRYINDEX, other.ref);
			ref = luaL_ref(l, LUA_REGISTRYINDEX);
		}
	}

	LuaRef::LuaRef(LuaRef&& other) :
		l(other.l),
		ref(other.ref)
	{
		other.l = nullptr;
		other.ref = LUA_REFNIL;
	}

	LuaRef& LuaRef::operator=(const LuaRef& other)
	{
		if (this != &other)
		{
			Clear();
			l = other.l;
			if (l != nullptr)
			{
				lua_rawgeti(l, LUA_REGISTRYINDEX, other.ref);
				ref = luaL_ref(l, LUA_REGISTRYINDEX);
			}
		}
		return *this;
	}

	LuaRef& LuaRef::operator=(LuaRef&& other)
	{
		l = other.l;
		ref = other.ref;
		other.l = nullptr;
		other.ref = LUA_REFNIL;
		return *this;
	}

	LuaRef::~LuaRef()
	{
		Clear();
	}

	bool LuaRef::operator==(const LuaRef& ref_) const
	{
		if (ref == ref_.ref)
			return true;
		if (IsEmpty() || ref_.IsEmpty()) 
			return false;

		Push();
		ref_.Push();
		bool result = lua_compare(l, -1, -2, LUA_OPEQ) != 0;
		lua_pop(l, 2);
		return result;
	}

	bool LuaRef::operator!=(const LuaRef& ref_) const
	{
		return !(*this == ref_);
	}

	LuaRef LuaRef::CreateRef(lua_State* l)
	{
		return LuaRef(l, luaL_ref(l, LUA_REGISTRYINDEX));
	}

	LuaRef LuaRef::CreateRef(lua_State* l, int index)
	{
		lua_pushvalue(l, index);
		return LuaRef(l, luaL_ref(l, LUA_REGISTRYINDEX));
	}

	LuaRef LuaRef::CreateGlobalRef(lua_State* l)
	{
		lua_pushglobaltable(l);
		return CreateRef(l);
	}

	LuaRef LuaRef::CreateFromPtr(lua_State* l, void* ptr)
	{
		lua_pushlightuserdata(l, ptr);
		return CreateRef(l);
	}

	LuaRef LuaRef::CreateTable(lua_State* l, int narray, int nrec)
	{
		lua_createtable(l, narray, nrec);
		return CreateRef(l);
	}

	void LuaRef::Clear()
	{
		if (l != nullptr && ref != LUA_REFNIL && ref != LUA_NOREF) {
			luaL_unref(l, LUA_REGISTRYINDEX, ref);
		}

		l = nullptr;
		ref = LUA_REFNIL;
	}

	bool LuaRef::IsEmpty() const
	{
		return l == nullptr || (ref == LUA_REFNIL || ref == LUA_NOREF);
	}

	void LuaRef::SetMetatable(LuaRef& luaRef)
	{
		Push();
		luaRef.Push();
		lua_setmetatable(l, -2);
		lua_pop(l, 1);
	}

	void LuaRef::Push() const
	{
		if (IsEmpty()) 
			return;
		lua_rawgeti(l, LUA_REGISTRYINDEX, ref);
	}

	bool LoadBuffer(lua_State* l, const char* data, size_t size, const char* name)
	{
		if (luaL_loadbuffer(l, data, size, name) != 0) 
			return false;

		if (lua_pcall(l, 0, 0, 0) != 0)
			return false;

		return true;
	}

	void AddFunction(lua_State* l, const char* name, lua_CFunction function)
	{
		lua_pushglobaltable(l);
		lua_pushcfunction(l, function);
		lua_setfield(l, -2, name);
	}

	int traceback(lua_State* L) 
	{
		if (!lua_isstring(L, 1)) return 1;

		lua_getglobal(L, "debug");
		if (!lua_istable(L, -1)) {
			lua_pop(L, 1);
			return 1;
		}

		lua_getfield(L, -1, "traceback");
		if (!lua_isfunction(L, -1)) {
			lua_pop(L, 2);
			return 1;
		}

		lua_pushvalue(L, 1);
		lua_pushinteger(L, 2);
		lua_call(L, 2, 1);

		return 1;
	}

	bool Execute(lua_State* l, Span<const char> content, const char* name, int nresults)
	{
		lua_pushcfunction(l, traceback);
		if (luaL_loadbuffer(l, content.begin(), content.length(), name) != 0)
		{
			Logger::Error("%s: %s", name, lua_tostring(l, -1));
			lua_pop(l, 2);
			return false;
		}

		if (lua_pcall(l, 0, nresults, -2) != 0) {
			Logger::Error(lua_tostring(l, -1));
			lua_pop(l, 2);
			return false;
		}
		lua_pop(l, 1);
		return true;
	}
}