#include "luaConfig.h"

namespace VulkanTest
{
    LuaConfig::LuaConfig()
    {
        luaState = luaL_newstate();
        stateOwned = true;
    }

    LuaConfig::LuaConfig(lua_State* l)
    {
        luaState = l;
        stateOwned = false;
    }

    LuaConfig::~LuaConfig()
    {
        if (stateOwned)
            lua_close(luaState);
    }

    void LuaConfig::AddFunc(const char* name, lua_CFunction func)
    {
        lua_pushglobaltable(luaState);
        lua_pushcfunction(luaState, func);
        lua_setfield(luaState, -2, name);
    }

    void LuaConfig::AddLightUserdata(const char* name, void* ptr)
    {
        lua_pushlightuserdata(luaState, ptr);
        lua_setglobal(luaState, name);
    }

    bool LuaConfig::Load(Span<const char> content)
    {
        if (!LuaUtils::Execute(luaState, content, "LuaConfig", 0))
            return false;
        return true;
    }
}