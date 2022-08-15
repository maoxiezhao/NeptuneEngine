#include "luaBindMeta.h"

namespace VulkanTest::LuaUtils::Binder
{
	bool LuaBindClassMetaBase::InitClassMeta(LuaRef& meta, LuaRef& parentMeta, const String& name, void* classID)
	{
		LuaRef ref = parentMeta.RawGet(name);
		if (!ref.IsEmpty())
		{
			meta = ref;
			return true;
		}

		lua_State* l = parentMeta.GetState();

		// Create global unique class id
		LuaRef classIDRef = LuaRef::CreateFromPtr(l, classID);
		LuaRef registry = LuaRef::CreateRef(l, LUA_REGISTRYINDEX);
		if (!registry.RawGet(classIDRef).IsEmpty())
		{
			Logger::Warning("Lua class is already registered %s", name.c_str());
			return false;
		}

		// Create class metatable
		LuaRef classTable = LuaRef::CreateTable(l);
		classTable.SetMetatable(classTable);
		classTable.RawSet("__index", &LuaObject::MetaIndex);
		classTable.RawSet("__newindex", &LuaObject::MetaNewIndex);
		classTable.RawSet("___getters", LuaRef::CreateTable(l));
		classTable.RawSet("___setters", LuaRef::CreateTable(l));
		classTable.RawSet("___type", name);
		classTable.RawSetp(ClassIDGenerator<LuaObject>::ID(), classIDRef);	// Used for checking

		// Register class in REGISTRYINDEX
		registry.RawSet(classIDRef, classTable);

		LuaRef staticClassTable = LuaRef::CreateTable(l);
		staticClassTable.RawSet("__CLASS", classTable);
		parentMeta.RawSet(name, staticClassTable);

		meta = staticClassTable;
		return true;
	}

	void LuaBindClassMetaBase::RegisterMethod(const String& name, const LuaRef& func)
	{
		meta.RawGet("__CLASS").RawSet(name, func);
	}

	void LuaBindClassMetaBase::RegisterStaticFunction(const String& name, const LuaRef& func)
	{
		meta.RawSet(name, func);
	}
}