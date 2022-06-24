#pragma once

#include "luaType.h"

namespace VulkanTest::LuaUtils
{
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

		void Clear();
		void Push()const;
		bool IsEmpty()const;

		static LuaRef NULL_REF;
	private:
		lua_State* l;
		int ref;
	};

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
	inline T Opt(lua_State* l, int index)
	{
		return LuaType<T>::Opt(l, index);
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
}