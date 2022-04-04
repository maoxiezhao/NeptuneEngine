#pragma once

#include "vMath.h"

namespace VulkanTest
{

///////////////////////////////////////////////////////////////////////////////////////////////////

#define VMATH_DEFINE_ARITH_OP(op) \
template <typename T> inline TVec2<T> operator op(const TVec2<T> &a, const TVec2<T> &b) { return TVec2<T>(a.x op b.x, a.y op b.y); }                \
template <typename T> inline TVec3<T> operator op(const TVec3<T> &a, const TVec3<T> &b) { return TVec3<T>(a.x op b.x, a.y op b.y, a.z op b.z); }    \
template <typename T> inline TVec4<T> operator op(const TVec4<T> &a, const TVec4<T> &b) { return TVec4<T>(a.x op b.x, a.y op b.y, a.z op b.z, a.w op b.w); }

VMATH_DEFINE_ARITH_OP(+)
VMATH_DEFINE_ARITH_OP(-)
VMATH_DEFINE_ARITH_OP(*)
VMATH_DEFINE_ARITH_OP(/)

#define VMATH_DEFINE_MOD_OP(op) \
template <typename T> inline TVec2<T>& operator op(TVec2<T> &a, const TVec2<T> &b) { a.x op b.x; a.y op b.y; return a; }                \
template <typename T> inline TVec3<T>& operator op(TVec3<T> &a, const TVec3<T> &b) { a.x op b.x; a.y op b.y; a.z op b.y; return a; }    \
template <typename T> inline TVec4<T>& operator op(TVec4<T> &a, const TVec4<T> &b) { a.x op b.x; a.y op b.y; a.z op b.y; a.w op b.w; return a; }    \
template <typename T> inline TVec2<T>& operator op(TVec2<T> &a, T b) { a.x op b; a.y op b; return a; }                                  \
template <typename T> inline TVec3<T>& operator op(TVec3<T> &a, T b) { a.x op b; a.y op b; a.z op b; return a; }                        \
template <typename T> inline TVec4<T>& operator op(TVec4<T> &a, T b) { a.x op b; a.y op b; a.z op b;  a.w op b; return a; }

VMATH_DEFINE_MOD_OP(+=)
VMATH_DEFINE_MOD_OP(-=)
VMATH_DEFINE_MOD_OP(*=)
VMATH_DEFINE_MOD_OP(/=)

template <typename T> inline TVec4<T> operator ==(const TVec2<T>& a, const TVec2<T>& b) { return a.x == b.x && a.y == b.y; }
template <typename T> inline TVec4<T> operator ==(const TVec3<T>& a, const TVec3<T>& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
template <typename T> inline TVec4<T> operator ==(const TVec4<T>& a, const TVec4<T>& b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
template <typename T> inline TVec4<T> operator !=(const TVec2<T>& a, const TVec2<T>& b) { return a.x != b.x || a.y != b.y; }
template <typename T> inline TVec4<T> operator !=(const TVec3<T>& a, const TVec3<T>& b) { return a.x != b.x || a.y != b.y || a.z != b.z; }
template <typename T> inline TVec4<T> operator !=(const TVec4<T>& a, const TVec4<T>& b) { return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w; }

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T> inline T dot(const TVec2<T> &a, const TVec2<T> &b) { return a.x * b.x + a.y * b.y; }
template <typename T> inline T dot(const TVec3<T> &a, const TVec3<T> &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
template <typename T> inline T dot(const TVec4<T> &a, const TVec4<T> &b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

template <typename T> 
inline float length(const T &v) { return sqrt(dot(v, v)); }

///////////////////////////////////////////////////////////////////////////////////////////////////

#define VMATH_DEFINE_MATRIX_SCALAR_OP(op) \
template <typename T> inline TMat2<T> operator op(const TMat2<T> &m, T s) { return TMat2<T>(m[0] op s, m[1] op s); } \
template <typename T> inline TMat3<T> operator op(const TMat3<T> &m, T s) { return TMat3<T>(m[0] op s, m[1] op s, m[2] op s); } \
template <typename T> inline TMat4<T> operator op(const TMat4<T> &m, T s) { return TMat4<T>(m[0] op s, m[1] op s, m[2] op s, m[3] op s); }
VMATH_DEFINE_MATRIX_SCALAR_OP(+)
VMATH_DEFINE_MATRIX_SCALAR_OP(-)
VMATH_DEFINE_MATRIX_SCALAR_OP(*)
VMATH_DEFINE_MATRIX_SCALAR_OP(/ )
}