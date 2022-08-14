#pragma once

#include "luaArgs.h"
#include "luaInvoke.h"

namespace VulkanTest::LuaUtils
{
namespace Binder
{
	template<typename T>
	struct ClassIDGenerator
	{
		static void* ID()
		{
			static bool id = false;
			return &id;
		}
	};

	class LuaObject
	{
	public:
		LuaObject() = default;
		virtual ~LuaObject() = default;

		template<typename T>
		static LuaObject* GetLuaObject(lua_State* l, int index)
		{
			auto classID = ClassIDGenerator<T>::ID();
			return GetLuaObject(l, index, classID);
		}

		template<typename T>
		static void* Allocate(lua_State* l, void* classID)
		{
			void* mem = lua_newuserdata(l, sizeof(T));
			lua_rawgetp(l, LUA_REGISTRYINDEX, classID);
			lua_setmetatable(l, -2);
			return mem;
		}

		static int MetaIndex(lua_State* l);
		static int MetaNewIndex(lua_State* l);

		virtual void* GetLuaObjectPtr() = 0;

	protected:
		static LuaObject* GetLuaObject(lua_State* l, int index, void* classID);
		static bool IsValid(lua_State* l, int index);
	};

	// Lua manager the lifetime of the object
	template<typename T>
	class LuaHandledObject : public LuaObject
	{
	public:
		LuaHandledObject() = default;
		~LuaHandledObject()
		{
			T* obj = static_cast<T*>(GetLuaObjectPtr());
			obj->~T();
		}

		template<typename... Args>
		static void Push(lua_State* l, LuaArgTuple<Args...>& args)
		{
			auto classID = ClassIDGenerator<T>::ID();
			void* mem = Allocate<LuaHandledObject<T>>(l, classID);
			LuaHandledObject<T>* obj = new(mem) LuaHandledObject<T>();
			Invoke::ClassConstructor<T>::Call(obj->GetLuaObjectPtr(), args.tuple);
		}

		static void Push(lua_State* l, const T& obj_)
		{
			auto classID = ClassIDGenerator<T>::ID();
			void* mem = Allocate<LuaHandledObject<T>>(l, classID);
			LuaHandledObject<T>* obj = new(mem) LuaHandledObject<T>();
			new(obj->GetLuaObjectPtr()) T(obj_);
		}

		void* GetLuaObjectPtr()override
		{
			return &data[0];
		}

	private:
		U8 data[sizeof(T)];
	};

	// Cpp manager the lifetime of the object
	template<typename T>
	class LuaObjectPtr : public LuaObject
	{
	public:
		LuaObjectPtr(void* ptr_) :
			ptr(ptr_)
		{
		}

		static void Push(lua_State* l, T* obj)
		{
			auto classID = ClassIDGenerator<T>::ID();
			void* mem = Allocate<LuaObjectPtr<T>>(l, classID);
			new(mem) LuaObjectPtr<T>(obj);
		}

		void* GetLuaObjectPtr()override
		{
			return ptr;
		}

	private:
		void* ptr;
	};

	template<typename T, typename ARGS>
	struct LuaObjectConstructor;

	template<typename T, typename...Args>
	struct LuaObjectConstructor<T, _lua_args(*)(Args...)>
	{
		static int Call(lua_State* l)
		{
			return ExceptionBoundary(l, [&] {
				LuaArgTuple<Args...> args;
				args.Get(l, 1);
				LuaHandledObject<T>::Push(l, args);
				return 1;
			});
		}
	};

	template<typename T>
	struct LuaObjectDestructor
	{
		static int Call(lua_State* l)
		{
			return ExceptionBoundary(l, [&] {
				LuaObject* obj = LuaObject::GetLuaObject<T>(l, 1);
				if (obj)
					obj->~LuaObject();
				return 0;
			});
		}
	};
}

template<typename T, bool IsRef>
struct LuaTypeClassMapping
{
	static void Push(lua_State* l, const T& value)
	{
		if constexpr (IsRef)
			Binder::LuaHandledObject<T>::Push(l, value);
		else
			Binder::LuaObjectPtr<T>::Push(l, const_cast<T*>(&value));
	}

	static T& Get(lua_State* l, int index)
	{
		Binder::LuaObject* obj = Binder::LuaObject::GetLuaObject<T>(l, index);
		return *static_cast<T*>(obj->GetLuaObjectPtr());
	}

	static const T& Opt(lua_State* l, int index, const T& defValue)
	{
		return lua_isnoneornil(l, index) ? defValue : Get(l, index);
	}
};
}