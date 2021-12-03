#pragma once

#include <stddef.h>
#include <utility>
#include <memory>
#include <atomic>
#include <type_traits>

namespace VulkanTest
{
namespace Util
{
	class SingleThreadCounter
	{
	public:
		inline void AddRef()
		{
			count++;
		}

		inline bool Release()
		{
			return --count == 0;
		}

	private:
		size_t count = 1;
	};

	class MultiThreadCounter
	{
	public:
		MultiThreadCounter()
		{
			count.store(1, std::memory_order_relaxed);
		}

		inline void AddRef()
		{
			count.fetch_add(1, std::memory_order_relaxed);
		}

		inline bool Release()
		{
			auto result = count.fetch_sub(1, std::memory_order_acq_rel);
			return result == 1;
		}

	private:
		std::atomic_size_t count;
	};

	template <typename T>
	class IntrusivePtr;

	template <typename T, typename Deleter = std::default_delete<T>, typename ReferenceOps = SingleThreadCounter>
	class IntrusivePtrEnabled
	{
	public:
		using IntrusivePtrType = IntrusivePtr<T>;
		using EnabledBase = T;
		using EnabledDeleter = Deleter;
		using EnabledReferenceOp = ReferenceOps;

		void Release()
		{
			if (refCount.Release())
				Deleter()(static_cast<T*>(this));
		}

		void AddReference()
		{
			refCount.AddRef();
		}

		IntrusivePtrEnabled() = default;
		IntrusivePtrEnabled(const IntrusivePtrEnabled&) = delete;
		void operator=(const IntrusivePtrEnabled&) = delete;

	protected:
		Util::IntrusivePtr<T> reference_from_this()
		{
			AddReference();
			return IntrusivePtr<T>(static_cast<T*>(this));
		}

	private:
		ReferenceOps refCount;
	};

	template <typename T>
	class IntrusivePtr
	{
	public:
		template <typename U>
		friend class IntrusivePtr;

		IntrusivePtr() = default;

		explicit IntrusivePtr(T* handle)
			: data(handle)
		{
		}

		T& operator*()
		{
			return *data;
		}

		const T& operator*() const
		{
			return *data;
		}

		T* operator->()
		{
			return data;
		}

		const T* operator->() const
		{
			return data;
		}

		explicit operator bool() const
		{
			return data != nullptr;
		}

		bool operator==(const IntrusivePtr& other) const
		{
			return data == other.data;
		}

		bool operator!=(const IntrusivePtr& other) const
		{
			return data != other.data;
		}

		T* get()
		{
			return data;
		}

		const T* get() const
		{
			return data;
		}

		void reset()
		{
			using ReferenceBase = IntrusivePtrEnabled<
				typename T::EnabledBase,
				typename T::EnabledDeleter,
				typename T::EnabledReferenceOp>;
			ReferenceBase* refData = data != nullptr ? static_cast<ReferenceBase*>(data) : nullptr;
			if (refData)
				refData->Release();
			data = nullptr;
		}

		template <typename U>
		IntrusivePtr& operator=(const IntrusivePtr<U>& other)
		{
			static_assert(std::is_base_of<T, U>::value,
				"Cannot safely assign downcasted intrusive pointers.");

			using ReferenceBase = IntrusivePtrEnabled<
				typename T::EnabledBase,
				typename T::EnabledDeleter,
				typename T::EnabledReferenceOp>;

			reset();
			data = static_cast<T*>(other.data);
			
			ReferenceBase* refData = static_cast<ReferenceBase*>(data);
			if (refData)
				refData->AddReference();

			return *this;
		}

		IntrusivePtr& operator=(const IntrusivePtr& other)
		{
			using ReferenceBase = IntrusivePtrEnabled<
				typename T::EnabledBase,
				typename T::EnabledDeleter,
				typename T::EnabledReferenceOp>;

			if (this != &other)
			{
				reset();
				data = other.data;
				if (data)
					static_cast<ReferenceBase*>(data)->AddReference();
			}
			return *this;
		}

		template <typename U>
		IntrusivePtr(const IntrusivePtr<U>& other)
		{
			*this = other;
		}

		IntrusivePtr(const IntrusivePtr& other)
		{
			*this = other;
		}

		~IntrusivePtr()
		{
			reset();
		}

		template <typename U>
		IntrusivePtr& operator=(IntrusivePtr<U>&& other) noexcept
		{
			reset();
			data = other.data;
			other.data = nullptr;
			return *this;
		}

		IntrusivePtr& operator=(IntrusivePtr&& other) noexcept
		{
			if (this != &other)
			{
				reset();
				data = other.data;
				other.data = nullptr;
			}
			return *this;
		}

		template <typename U>
		IntrusivePtr(IntrusivePtr<U>&& other) noexcept
		{
			*this = std::move(other);
		}

		template <typename U>
		IntrusivePtr(IntrusivePtr&& other) noexcept
		{
			*this = std::move(other);
		}

	private:
		T* data = nullptr;
	};


	template <typename Derived>
	using DerivedIntrusivePtrType = IntrusivePtr<Derived>;

	template <typename T, typename... P>
	DerivedIntrusivePtrType<T> make_handle(P &&... p)
	{
		return DerivedIntrusivePtrType<T>(new T(std::forward<P>(p)...));
	}

	template <typename Base, typename Derived, typename... P>
	typename Base::IntrusivePtrType make_derived_handle(P &&... p)
	{
		return typename Base::IntrusivePtrType(new Derived(std::forward<P>(p)...));
	}

	template <typename T>
	using ThreadSafeIntrusivePtrEnabled = IntrusivePtrEnabled<T, std::default_delete<T>, MultiThreadCounter>;
}
}