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
		std::swap(l, other.l);
		std::swap(ref, other.ref);
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

	bool Execute(lua_State* l, Span<const char> content, const char* name, int nresults)
	{
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