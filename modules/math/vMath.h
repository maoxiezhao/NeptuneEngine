#pragma once

#include "math_common.h"

namespace VulkanTest
{
#if defined(_MSC_VER) && (_MSC_FULL_VER < 190023506)
#define VEC_CONST const
#define VEC_CONSTEXPR
#else
#define VEC_CONST constexpr
#define VEC_CONSTEXPR constexpr
#endif

	template <typename T> struct TMat2;
	template <typename T> struct TMat3;
	template <typename T> struct TMat4;

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

		VEC_CONSTEXPR TVec2(T x_, T y_) noexcept
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

		VEC_CONSTEXPR TVec3(T x_, T y_, T z_) noexcept
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

		VEC_CONSTEXPR TVec4(T x_, T y_, T z_, T w_) noexcept
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

		inline TVec3<T> xyz() const;
	};


	template <typename T>
	struct TMat2
	{
		TMat2() = default;

		explicit inline TMat2(T v) noexcept
		{
			vec[0] = TVec2<T>(v, T(0));
			vec[1] = TVec2<T>(T(0), v);
		}

		inline TMat2(const TVec2<T>& a, const TVec2<T>& b) noexcept
		{
			vec[0] = a;
			vec[1] = b;
		}

		inline TVec2<T>& operator[](int index)
		{
			return vec[index];
		}

		inline const TVec2<T>& operator[](int index) const
		{
			return vec[index];
		}

	private:
		TVec2<T> vec[2];
	};

	template <typename T>
	struct TMat3
	{
		TMat3() = default;

		explicit inline TMat3(T v) noexcept
		{
			vec[0] = TVec3<T>(v, T(0), T(0));
			vec[1] = TVec3<T>(T(0), v, T(0));
			vec[2] = TVec3<T>(T(0), T(0), v);
		}

		inline TMat3(const TVec3<T>& a, const TVec3<T>& b, const TVec3<T>& c) noexcept
		{
			vec[0] = a;
			vec[1] = b;
			vec[2] = c;
		}

		explicit inline TMat3(const TMat4<T>& m) noexcept
		{
			for (int col = 0; col < 3; col++)
				for (int row = 0; row < 3; row++)
					vec[col][row] = m[col][row];
		}

		inline TVec3<T>& operator[](int index)
		{
			return vec[index];
		}

		inline const TVec3<T>& operator[](int index) const
		{
			return vec[index];
		}

	private:
		TVec3<T> vec[3];
	};

	template <typename T>
	struct TMat4
	{
		TMat4() = default;

		TMat4(T m00, T m01, T m02, T m03,
			T m10, T m11, T m12, T m13,
			T m20, T m21, T m22, T m23,
			T m30, T m31, T m32, T m33)
			: _11(m00), _12(m01), _13(m02), _14(m03),
			_21(m10), _22(m11), _23(m12), _24(m13),
			_31(m20), _32(m21), _33(m22), _34(m23),
			_41(m30), _42(m31), _43(m32), _44(m33) {}

		explicit inline TMat4(T v) noexcept
		{
			vec[0] = TVec4<T>(v, T(0), T(0), T(0));
			vec[1] = TVec4<T>(T(0), v, T(0), T(0));
			vec[2] = TVec4<T>(T(0), T(0), v, T(0));
			vec[3] = TVec4<T>(T(0), T(0), T(0), v);
		}

		explicit inline TMat4(const TMat3<T>& m) noexcept
		{
			vec[0] = TVec4<T>(m[0], T(0));
			vec[1] = TVec4<T>(m[1], T(0));
			vec[2] = TVec4<T>(m[2], T(0));
			vec[3] = TVec4<T>(T(0), T(0), T(0), T(1));
		}

		inline TMat4(const TVec4<T>& a, const TVec4<T>& b, const TVec4<T>& c, const TVec4<T>& d) noexcept
		{
			vec[0] = a;
			vec[1] = b;
			vec[2] = c;
			vec[3] = d;
		}

		inline TVec4<T>& operator[](int index)
		{
			return vec[index];
		}

		inline const TVec4<T>& operator[](int index) const
		{
			return vec[index];
		}

		union
		{
			struct
			{
				T _11, _12, _13, _14;
				T _21, _22, _23, _24;
				T _31, _32, _33, _34;
				T _41, _42, _43, _44;
			};
			TVec4<T> vec[4];
		};
	};

	using F32x2 = TVec2<F32>;
	using F32x3 = TVec3<F32>;
	using F32x4 = TVec4<F32>;

	using I32x2 = TVec2<I32>;
	using I32x3 = TVec3<I32>;
	using I32x4 = TVec4<I32>;

	using U32x2 = TVec2<U32>;
	using U32x3 = TVec3<U32>;
	using U32x4 = TVec4<U32>;

	using D32x2 = TVec2<F64>;
	using D32x3 = TVec3<F64>;
	using D32x4 = TVec4<F64>;

	using FMat2x2 = TMat2<F32>;
	using FMat3x3 = TMat3<F32>;
	using FMat4x4 = TMat4<F32>;

	using IMat2x2 = TMat2<I32>;
	using IMat3x3 = TMat3<I32>;
	using IMat4x4 = TMat4<I32>;

	using UMat2x2 = TMat2<U32>;
	using UMat3x3 = TMat3<U32>;
	using UMat4x4 = TMat4<U32>;

#define VMATH_IMPL_SWIZZLE(ret_type, self_type, swiz, ...) template <typename T> T##ret_type<T> T##self_type<T>::swiz() const { return T##ret_type<T>(__VA_ARGS__); }

VMATH_IMPL_SWIZZLE(Vec3, Vec4, xyz, x, y, z)

}