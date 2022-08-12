#pragma once

#include "core\common.h"
#include "core\collections\array.h"

namespace VulkanTest
{
	template <typename T>
	struct Delegate;

	template<typename R, typename...Args>
	class Delegate<R(Args...)>
	{
	private:
		template<R (*Func)(Args...)>
		static R StaticFunction(void*, Args... args)
		{
			return (Func)(args...);
		}

		template<typename C, R(C::*Func)(Args...)>
		static R ClassMethod(void* ptr, Args... args)
		{
			return (static_cast<C*>(ptr)->*Func)(args...);
		}

		template<typename C, R(C::* Func)(Args...)const>
		static R ClassMethod(void* ptr, Args... args)
		{
			return (static_cast<C*>(ptr)->*Func)(args...);
		}

	public:
		Delegate() :
			instance(nullptr),
			func(nullptr)
		{}

		template <R(*Func)(Args...)>
		void Bind()
		{
			instance = nullptr;
			func = &StaticFunction<Func>;
		}

		template <auto Func, typename C>
		void Bind(C* ptr)
		{
			instance = ptr;
			func = &ClassMethod<C, Func>;
		}

		R Invoke(Args... args)const
		{
			ASSERT(func != nullptr);
			return func(instance, args...);
		}

		bool operator==(const Delegate<R(Args...)>& rhs)
		{
			return instance == rhs.instance && func == rhs.func;
		}

	private:
		using InstancePtr = void*;
		using InstanceFunc = R(*)(void*, Args...);

		InstancePtr instance;
		InstanceFunc func;
	};

	template <typename T> 
	struct DelegateList;

	template<typename R, typename...Args>
	class DelegateList<R(Args...)>
	{
	public:
		using DelegateT = Delegate<R(Args...)>;

		DelegateList() = default;

		template <R(*Func)(Args...)>
		void Bind()
		{
			DelegateT cb;
			cb.Bind<Func>();
			ScopedMutex lock(mutex);
			delegates.push_back(cb);
		}

		template <auto Func, typename C>
		void Bind(C* ptr)
		{
			DelegateT cb;
			cb.Bind<Func>(ptr);
			ScopedMutex lock(mutex);
			delegates.push_back(cb);
		}

		template <R(*Func)(Args...)>
		void Unbind()
		{
			DelegateT cb;
			cb.Bind<Func>();
			ScopedMutex lock(mutex);
			for (U32 i = 0; i < delegates.size(); i++)
			{
				if (delegates[i] == cb)
				{
					delegates.swapAndPop(i);
					break;
				}
			}
		}

		template <auto Func, typename C>
		void Unbind(C* ptr)
		{
			DelegateT cb;
			cb.Bind<Func>(ptr);
			ScopedMutex lock(mutex);
			for (U32 i = 0; i < delegates.size(); i++)
			{
				if (delegates[i] == cb)
				{
					delegates.swapAndPop(i);
					break;
				}
			}
		}

		void Invoke(Args... args)
		{
			Array<DelegateT> temp;
			{
				ScopedMutex lock(mutex);
				for (auto& d : delegates)
					temp.push_back(d);
			}

			for (auto& d : temp)
				d.Invoke(args...);
		}

	private:
		Array<DelegateT> delegates;
		Mutex mutex;
	};
}