#pragma once

#include "luaArgs.h"
#include "luaObject.h"

namespace VulkanTest::LuaUtils
{
namespace Binder
{
	class ILuaBindBase
	{
	public:
		ILuaBindBase(const LuaRef& meta_) : 
			meta(meta_) 
		{
		}

		lua_State* GetLuaState() {
			return meta.GetState();
		}

	protected:
		LuaRef meta;
	};

	class LuaBindClassMetaBase : public ILuaBindBase
	{
	public:
		LuaBindClassMetaBase(const LuaRef& meta_) : ILuaBindBase(meta_) {}

	protected:
		static bool InitClassMeta(LuaRef& meta, LuaRef& parentMeta, const String& name, void* classID);
	};

	template<typename T, typename ParentT>
	class LuaBindClassMeta : public LuaBindClassMetaBase
	{
	public:
		using ClassMetaType = LuaBindClassMeta<T, ParentT>;

		static ClassMetaType BindClass(lua_State* l, LuaRef& parentMeta, const String& name)
		{
			LuaRef classMeta = LuaRef::NULL_REF;
			if (!InitClassMeta(classMeta, parentMeta, name, ClassIDGenerator<T>::ID()))
			{
				Logger::Warning("Failed to bind lua class meta ", name.c_str());
				return ClassMetaType(LuaRef::NULL_REF);
			}

			if (classMeta == LuaRef::NULL_REF)
			{
				Logger::Warning("Failed to bind lua class mtea", name.c_str());
				return ClassMetaType(LuaRef::NULL_REF);
			}

			classMeta.RawGet("__CLASS").RawSet("__gc", &LuaObjectDestructor<T>::Call);
			return ClassMetaType(classMeta);
		}

	public:
		template<typename Args>
		ClassMetaType& AddConstructor(Args&& args)
		{
			if (meta == LuaRef::NULL_REF)
				return *this;

			// Tow ways to construct object
			// 1. local obj = CLAZZ(...);
			// 2. local obj = CLAZZ:new(...);
			meta.RawSet("__call", LuaObjectConstructor<T, Args>::Call);
			meta.RawSet("new", LuaObjectConstructor<T, Args>::Call);
			return *this;
		}

		template<auto Func>
		ClassMetaType& AddFunction()
		{
			if (meta == LuaRef::NULL_REF)
				return *this;

			return *this;
		}

		ParentT EndClass()
		{
			return ParentT(GetLuaState());
		}

	protected:
		LuaBindClassMeta(const LuaRef& meta_) : LuaBindClassMetaBase(meta_) {}
	};
}
}