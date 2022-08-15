#pragma once

#include "luaArgs.h"
#include "luaObject.h"

namespace VulkanTest::LuaUtils
{
namespace Binder
{
	template<auto Func>
	struct ClassStaticFunction
	{
		using FunctionHelper = Invoke::function_helper_t<decltype(Func)>;

		static int Call(lua_State* l)
		{	
			return ExceptionBoundary(l, [&] {
				return Invoke::StaticMethodCaller<Func>::Call(
					l,
					FunctionHelper::args(l, 2).tuple
				);
			});
		}
	};

	template<typename T, auto Func>
	struct ClassMethodFunction
	{
		using FunctionHelper = Invoke::function_helper_t<decltype(Func)>;

		static int Call(lua_State* l)
		{
			return ExceptionBoundary(l, [&] {
				return Invoke::ClassMethodCaller<T, Func>::Call(
					l, 
					LuaObject::GetObject<T>(l, 1), 
					FunctionHelper::args(l, 2).tuple
				);
			});
		}
	};

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

		void RegisterMethod(const String& name, const LuaRef& func);
		void RegisterStaticFunction(const String& name, const LuaRef& func);
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
			// Tow ways to construct object
			// 1. local obj = CLAZZ(...);
			// 2. local obj = CLAZZ:new(...);

			if (meta == LuaRef::NULL_REF)
				return *this;

			meta.RawSet("__call", LuaObjectConstructor<T, Args>::Call);
			meta.RawSet("new", LuaObjectConstructor<T, Args>::Call);
			return *this;
		}

		template<auto Func>
		ClassMetaType& AddMethod(const String& name)
		{
			static_assert(std::is_member_function_pointer_v<decltype(Func)>);
			if (meta == LuaRef::NULL_REF)
				return *this;

			using MethodFunction = ClassMethodFunction<T, Func>;
			LuaRef funcRef = LuaRef::CreateFunction(GetLuaState(), &MethodFunction::Call);
			RegisterMethod(name, funcRef);
			return *this;
		}

		template<auto Func>
		ClassMetaType& AddFunction(const String& name)
		{
			static_assert(!std::is_member_function_pointer_v<decltype(Func)>);
			if (meta == LuaRef::NULL_REF)
				return *this;

			using StaticFunction = ClassStaticFunction<Func>;
			LuaRef funcRef = LuaRef::CreateFunction(GetLuaState(), &StaticFunction::Call);
			RegisterStaticFunction(name, funcRef);
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