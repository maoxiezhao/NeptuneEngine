#pragma once

#include "core\common.h"
#include "core\collections\array.h"
#include "core\platform\atomic.h"

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

	public:
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
		using InstancePtr = void*;
		using InstanceFunc = R(*)(void*, Args...);

		DelegateList()
		{
		}

		~DelegateList()
		{
			auto ptr = (DelegateT*)ptrs_;
			if (ptr)
				CJING_FREE((void*)ptr);
		}

		template <R(*Func)(Args...)>
		void Bind()
		{
			DelegateT cb;
			cb.Bind<Func>();
			Bind(cb);
		}

		template <auto Func, typename C>
		void Bind(C* ptr)
		{
			DelegateT cb;
			cb.Bind<Func>(ptr);
			Bind(cb);
		}

		void Bind(const DelegateT& cb)
		{
			const intptr size = AtomicRead(&size_);
			DelegateT* bindings = (DelegateT*)AtomicRead(&ptrs_);
			if (bindings)
			{
				// Find a free slot
				for (intptr i = 0; i < size; i++)
				{
					if (AtomicCmpExchange((intptr volatile*)&bindings[i].func, (intptr)cb.func, 0) == 0)
					{
						bindings[i].instance = cb.instance;
						return;
					}
				}
			}

			// Allocate a new bindings
			const intptr newSize = size ? size * 2 : 32;
			auto newBindings = (DelegateT*)CJING_MALLOC(newSize * sizeof(DelegateT));
			memcpy(newBindings, bindings, size * sizeof(DelegateT));
			memset(newBindings + size, 0, (newSize - size) * sizeof(DelegateT));

			newBindings[size] = cb;

			auto oldBindings = (DelegateT*)AtomicCmpExchange(&ptrs_, (intptr)newBindings, (intptr)bindings);
			if (oldBindings != bindings)
			{
				// Othrer thread already set the new bindings, free new bindings and try again
				CJING_FREE(newBindings);
				Bind(cb);
			}
			else
			{
				AtomicExchange(&size_, newSize);
				CJING_SAFE_DELETE(bindings);
			}
		}

		template <R(*Func)(Args...)>
		void Unbind()
		{
			DelegateT cb;
			cb.Bind<Func>();
			UnBind(cb);
		}

		template <auto Func, typename C>
		void Unbind(C* ptr)
		{
			DelegateT cb;
			cb.Bind<Func>(ptr);
			UnBind(cb);
		}

		void UnBind(DelegateT& cb)
		{
			const intptr size = AtomicRead(&size_);
			DelegateT* bindings = (DelegateT*)AtomicRead(&ptrs_);
			for (intptr i = 0; i < size; i++)
			{
				if (AtomicRead((intptr volatile*)&bindings[i].instance) == (intptr)cb.instance &&
					AtomicRead((intptr volatile*)&bindings[i].func) == (intptr)cb.func)
				{
					AtomicExchange((intptr volatile*)&bindings[i].instance, 0);
					AtomicExchange((intptr volatile*)&bindings[i].func, 0);
					break;
				}
			}

			if ((DelegateT*)AtomicRead(&ptrs_) != bindings)
			{
				// Othrer thread already set the new bindings, unbind again
				UnBind(cb);
			}
		}

		void Invoke(Args... args)
		{
			const intptr size = AtomicRead(&size_);
			DelegateT* bindings = (DelegateT*)AtomicRead(&ptrs_);
			for (intptr i = 0; i < size; i++)
			{
				auto func = (InstanceFunc)AtomicRead((intptr volatile*)&bindings[i].func);
				if (func != nullptr)
				{
					auto inst = (void*)AtomicRead((intptr volatile*)&bindings[i].instance);
					func(inst, std::forward<Args>(args)...);
				}
			}
		}

	private:
		intptr volatile ptrs_ = 0;
		intptr volatile size_ = 0;
	};
}