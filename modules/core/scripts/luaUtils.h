#pragma once

#include "luaType.h"

namespace VulkanTest::LuaUtils
{
	bool LoadBuffer(lua_State* l, const char* data, size_t size, const char* name);
	void AddFunction(lua_State* l, const char* name, lua_CFunction function);

	template<typename T>
	inline void Push(lua_State* l, const T& v)
	{
		LuaType<T>::Push(l, v);
	}

	template<typename T>
	inline T Get(lua_State* l, int index)
	{
		return LuaType<T>::Get(l, index);
	}

	template<typename T>
	inline T GetField(lua_State* l, const char* name, int index = -1)
	{
		lua_getfield(l, index, name);
		T v =  LuaType<T>::Get(l, -1);
		lua_pop(l, 1);
		return v;
	}

	template<typename T>
	inline T RawGet(lua_State* l, int idx, int n)
	{
		lua_rawgeti(l, idx, n);
		T v = LuaType<T>::Get(l, -1);
		lua_pop(l, 1);
		return v;
	}

	template<typename T>
	inline T Opt(lua_State* l, int index, T defaultValue)
	{
		return LuaType<T>::Opt(l, index, defaultValue);
	}

	template<typename T>
	inline T OptField(lua_State* l, const char* name, T defaultValue, int index = -1)
	{
		lua_getfield(l, index, name);
		T v = LuaType<T>::Opt(l, -1, defaultValue);
		lua_pop(l, 1);
		return v;
	}

	bool Execute(lua_State* l, Span<const char> content, const char* name, int nresults);

	template<typename T>
	inline bool Check(lua_State* l, int index)
	{
		return LuaType<T>::Check(l, index);
	}

	template<typename Callable>
	int ExceptionBoundary(lua_State* l, Callable&& func)
	{
		try
		{
			return func();
		}
		catch (const LuaException& ex)
		{
			Logger::Error(ex.what());
			luaL_error(l, ex.what());
		}
		catch (const std::exception& ex)
		{
			luaL_error(l, (std::string("Error:") + ex.what()).c_str());
		}
		return 0;
	}

	template <typename T, typename F>
	bool ForEachArrayItem(lua_State* l, int index, const char* errMsg, F&& func)
	{
		return ExceptionBoundary(l, [&] {
			if (!lua_istable(l, index))
			{
				if (errMsg)
					luaL_argerror(l, index, errMsg);

				return false;
			}

			const int n = (int)lua_rawlen(l, index);
			for (int i = 0; i < n; ++i)
			{
				lua_rawgeti(l, index, i + 1);
				if (Check<T>(l, -1))
				{
					func(Get<T>(l, -1));
				}
				else if (errMsg)
				{
					lua_pop(l, 1);
					luaL_argerror(l, index, errMsg);
				}
				lua_pop(l, 1);
			}
			return true;
		});
	}

	class LuaRef
	{
	public:
		LuaRef();
		LuaRef(lua_State* l, int ref_);
		LuaRef(const LuaRef& other);
		LuaRef(LuaRef&& other);
		~LuaRef();

		LuaRef& operator=(const LuaRef& other);
		LuaRef& operator=(LuaRef&& other);

		bool operator ==(const LuaRef& ref_)const;
		bool operator !=(const LuaRef& ref_)const;

		static LuaRef CreateRef(lua_State* l);
		static LuaRef CreateRef(lua_State* l, int index);
		static LuaRef CreateGlobalRef(lua_State* l);
		static LuaRef CreateFromPtr(lua_State* l, void* ptr);
		static LuaRef CreateTable(lua_State* l, int narray = 0, int nrec = 0);

		lua_State* GetState() {
			return l;
		}

		template<typename T>
		static std::enable_if_t<std::is_function<T>::value, LuaRef>
			CreateFunc(lua_State* l, lua_CFunction func, const T& userdata)
		{
			lua_pushcclosure(l, func, 1);
			return CreateRef(l);
		}

		template<typename T>
		static std::enable_if_t<!std::is_function<T>::value, LuaRef>
			CreateFunc(lua_State* l, lua_CFunction func, const T& userdata)
		{
			lua_pushcclosure(l, func, 1);
			return CreateRef(l);
		}

		template<typename V, typename K>
		void RawSet(const K& key, const V& value)
		{
			Push();
			LuaUtils::Push(l, key);
			LuaUtils::Push(l, value);
			lua_rawset(l, -3);
			lua_pop(l, 1);
		}

		template<typename V>
		void RawSetp(void* key, const V& value)
		{
			Push();
			LuaUtils::Push(l, value);
			lua_rawsetp(l, -2, key);
			lua_pop(l, 1);
		}

		template<typename V = LuaRef>
		V RawGetp(void* key)
		{
			Push();
			lua_rawgetp(l, -1, key);
			V v = LuaUtils::Get<V>(l, -1);
			lua_pop(l, 2);
			return v;
		}

		template<typename V = LuaRef, typename K>
		V RawGet(const K& key)
		{
			Push();
			LuaUtils::Push(l, key);
			lua_rawget(l, -2);
			V v = LuaUtils::Get<V>(l, -1);
			lua_pop(l, 2);
			return v;
		}

		void Clear();
		void Push()const;
		bool IsEmpty()const;
		void SetMetatable(LuaRef& luaRef);

		static LuaRef NULL_REF;
	private:
		lua_State* l;
		int ref;
	};

	template<>
	struct LuaTypeNormalMapping<LuaRef>
	{
		static void Push(lua_State* l, const LuaRef& value)
		{
			if (value.IsEmpty())
				lua_pushnil(l);
			else
				value.Push();
		}

		static LuaRef Get(lua_State* l, int index)
		{
			if (lua_isnil(l, index))
				return LuaRef::NULL_REF;
			else
				return LuaRef::CreateRef(l, index);
		}

		static LuaRef Opt(lua_State* l, int index, const LuaRef& defValue)
		{
			return lua_isnone(l, index) ? defValue : Get(l, index);
		}

		static bool Check(lua_State* l, int index)
		{
			return true;
		}
	};
}