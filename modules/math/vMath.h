#pragma once

#include "math_common.h"

namespace VulkanTest
{
	template<typename T>
	struct TVec2
	{
		union
		{
			T data[2];
			struct
			{
				T x, y;
			};
		};
	
		TVec2() = default;
		TVec2(const TVec2&) = default;

		template <typename U>
		explicit inline TVec2(const TVec2<U> &u) noexcept
		{
			x = T(u.x);
			y = T(u.y);
		}

		explicit TVec2(T v) noexcept
		{
			x = v;
			y = v;
		}

		TVec2(T x_, T y_) noexcept
		{
			x = x_;
			y = y_;
		}

		inline T& operator[](int index)
		{
			return data[index];
		}
		inline const T& operator[](int index) const
		{
			return data[index];
		}
	};

	template<typename T>
	struct TVec3
	{
		union
		{
			T data[3];
			struct
			{
				T x, y, z;
			};
		};

		TVec3() = default;
		TVec3(const TVec3&) = default;

		template <typename U>
		explicit inline TVec3(const TVec3<U> &u) noexcept
		{
			x = T(u.x);
			y = T(u.y);
			z = T(u.z);
		}

		explicit TVec3(T v) noexcept
		{
			x = v;
			y = v;
			z = v;
		}

		TVec3(T x_, T y_, T z_) noexcept
		{
			x = x_;
			y = y_;
			z = z_;
		}

		TVec3(const TVec2<T>& a, T z_) noexcept
		{
			x = a.x;
			y = a.y;
			z = z_;
		}

		inline T& operator[](int index)
		{
			return data[index];
		}
		inline const T& operator[](int index) const
		{
			return data[index];
		}
	};

	template<typename T>
	struct TVec4
	{
		union
		{
			T data[4];
			struct
			{
				T x, y, z, w;
			};
		};

		TVec4() = default;
		TVec4(const TVec4&) = default;

		template <typename U>
		explicit inline TVec4(const TVec4<U> &u) noexcept
		{
			x = T(u.x);
			y = T(u.y);
			z = T(u.z);
			w = T(u.w);
		}

		explicit TVec4(T v) noexcept
		{
			x = v;
			y = v;
			z = v;
			w = v;
		}

		TVec4(T x_, T y_, T z_, T w_) noexcept
		{
			x = x_;
			y = y_;
			z = z_;
			w = w_;
		}
		
		TVec4(const TVec3<T>& a, T w_) noexcept
		{
			x = a.x;
			y = a.y;
			z = a.z;
			w = w_;
		}

		inline T& operator[](int index)
		{
			return data[index];
		}
		inline const T& operator[](int index) const
		{
			return data[index];
		}
	};

	using Vec2 = TVec2<F32>;
	using Vec3 = TVec3<F32>;
	using Vec4 = TVec4<F32>;

	using IVec2 = TVec2<I32>;
	using IVec3 = TVec3<I32>;
	using IVec4 = TVec4<I32>;

	using DVec2 = TVec2<F64>;
	using DVec3 = TVec3<F64>;
	using DVec4 = TVec4<F64>;
}