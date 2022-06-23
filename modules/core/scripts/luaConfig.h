#pragma once

#include "luaUtils.h"

namespace VulkanTest
{
	class VULKAN_TEST_API LuaConfig
	{
	public:
		LuaConfig();
		~LuaConfig();

		void AddFunc(const char* name, lua_CFunction func);
		void AddLightUserdata(const char* name, void* ptr);

		bool Load(Span<const char> content);

		lua_State* GetState() {
			return luaState;
		}

	private:
		lua_State* luaState;
	};
}