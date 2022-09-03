#pragma once

#include "math_common.h"
#include "vMath.h"

#ifdef USE_DIRECTX_MATH
#include "directXMath.h"
#endif

namespace VulkanTest
{
	static const FMat4x4 IDENTITY_MATRIX = FMat4x4(1.0f);

	constexpr F32x3 Max(const F32x3& a, const F32x3& b) {
		return F32x3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
	}

	constexpr F32x3 Min(const F32x3& a, const F32x3& b) {
		return F32x3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
	}

	template <typename T> CJING_FORCE_INLINE T Signum(T a) {
		return a > 0 ? (T)1 : (a < 0 ? (T)-1 : 0);
	}

	inline F32 Distance(const VECTOR& v1, const VECTOR& v2)
	{
		VECTOR vectorSub = VectorSubtract(v1, v2);
		VECTOR length = Vector3Length(vectorSub);
		return StoreF32(length);
	}

	inline F32 DistanceSquared(const VECTOR& v1, const VECTOR& v2)
	{
		VECTOR vectorSub = VectorSubtract(v1, v2);
		VECTOR length = Vector3LengthSq(vectorSub);
		return StoreF32(length);
	}

	inline F32 Distance(const F32x2& v1, const F32x2& v2)
	{
		VECTOR vector1 = LoadF32x2(v1);
		VECTOR vector2 = LoadF32x2(v2);
		return VectorGetX(Vector2Length(VectorSubtract(vector2, vector1)));
	}

	inline F32 Distance(const F32x3& v1, const F32x3& v2)
	{
		VECTOR vector1 = LoadF32x3(v1);
		VECTOR vector2 = LoadF32x3(v2);
		return Distance(vector1, vector2);
	}

	inline F32 DistanceSquared(const F32x3& v1, const F32x3& v2)
	{
		VECTOR vector1 = LoadF32x3(v1);
		VECTOR vector2 = LoadF32x3(v2);
		return DistanceSquared(vector1, vector2);
	}

	constexpr F32 Lerp(F32 value1, F32 value2, F32 amount)
	{
		return value1 + (value2 - value1) * amount;
	}

	constexpr F32x2 Lerp(const F32x2& a, const F32x2& b, float i)
	{
		return F32x2(Lerp(a.x, b.x, i), Lerp(a.y, b.y, i));
	}

	constexpr F32x3 Lerp(const F32x3& a, const F32x3& b, float i)
	{
		return F32x3(Lerp(a.x, b.x, i), Lerp(a.y, b.y, i), Lerp(a.z, b.z, i));
	}

	constexpr F32x4 Lerp(const F32x4& a, const F32x4& b, float i)
	{
		return F32x4(Lerp(a.x, b.x, i), Lerp(a.y, b.y, i), Lerp(a.z, b.z, i), Lerp(a.w, b.w, i));
	}

	constexpr F32 Clamp(F32 val, F32 min, F32 max)
	{
		return std::min(max, std::max(min, val));
	}

	inline F32 GetAngle(const F32x3& a, const F32x3& b, const F32x3& axis)
	{
		VECTOR A = LoadF32x3(a);
		VECTOR B = LoadF32x3(b);
		F32 dp = Clamp(VectorGetX(Vector3Dot(A, B)), -1, 1);
		F32 angle = std::acos(dp);
		if (VectorGetX(Vector3Dot(Vector3Cross(A, B), LoadF32x3(axis))) < 0)	// Check side
			angle = MATH_2PI - angle;
		return angle;
	}

	struct Transform
	{
		F32x3 translation = F32x3(0.f, 0.f, 0.f);
		F32x4 rotation = F32x4(0.f, 0.f, 0.f, 1.f);	// quaternion
		F32x3 scale = F32x3(1.f, 1.f, 1.f);
		FMat4x4 world = IDENTITY_MATRIX;
		bool isDirty = false;
		bool SetDiryt() {
			isDirty = true;
		}

		void UpdateTransform();
		void Translate(const F32x3& value);
		void RotateRollPitchYaw(const F32x3& value);
		void Rotate(const F32x4& quaternion);
		void Scale(const F32x3& value);

		F32x3 DoTransform(const F32x3& value) const;
		F32x4 DoTransform(const F32x4& value) const;

		F32x3 GetPosition()const;
		MATRIX GetMatrix()const;
	};

	//-----------------------------------------------------------------------------
	// Compute the intersection of a ray (Origin, Direction) with a triangle 
	// (V0, V1, V2).  Return true if there is an intersection and also set *pDist 
	// to the distance along the ray to the intersection.
	// 
	// The algorithm is based on Moller, Tomas and Trumbore, "Fast, Minimum Storage 
	// Ray-Triangle Intersection", Journal of Graphics Tools, vol. 2, no. 1, 
	// pp 21-28, 1997.
	//-----------------------------------------------------------------------------
	_Use_decl_annotations_
	inline bool XM_CALLCONV RayTriangleIntersects(
			FXMVECTOR Origin,
			FXMVECTOR Direction,
			FXMVECTOR V0,
			GXMVECTOR V1,
			HXMVECTOR V2,
			F32& Dist,
			F32 TMin = 0,
			F32 TMax = std::numeric_limits<F32>::max()
		)
	{
		const XMVECTOR g_RayEpsilon = XMVectorSet(1e-20f, 1e-20f, 1e-20f, 1e-20f);
		const XMVECTOR g_RayNegEpsilon = XMVectorSet(-1e-20f, -1e-20f, -1e-20f, -1e-20f);

		XMVECTOR Zero = XMVectorZero();

		XMVECTOR e1 = XMVectorSubtract(V1, V0);
		XMVECTOR e2 = XMVectorSubtract(V2, V0);

		// p = Direction ^ e2;
		XMVECTOR p = XMVector3Cross(Direction, e2);

		// det = e1 * p;
		XMVECTOR det = XMVector3Dot(e1, p);

		XMVECTOR u, v, t;

		if (XMVector3GreaterOrEqual(det, g_RayEpsilon))
		{
			// Determinate is positive (front side of the triangle).
			XMVECTOR s = XMVectorSubtract(Origin, V0);

			// u = s * p;
			u = XMVector3Dot(s, p);

			XMVECTOR NoIntersection = XMVectorLess(u, Zero);
			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(u, det));

			// q = s ^ e1;
			XMVECTOR q = XMVector3Cross(s, e1);

			// v = Direction * q;
			v = XMVector3Dot(Direction, q);

			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(v, Zero));
			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(XMVectorAdd(u, v), det));

			// t = e2 * q;
			t = XMVector3Dot(e2, q);

			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(t, Zero));

			if (XMVector4EqualInt(NoIntersection, XMVectorTrueInt()))
			{
				Dist = 0.f;
				return false;
			}
		}
		else if (XMVector3LessOrEqual(det, g_RayNegEpsilon))
		{
			// Determinate is negative (back side of the triangle).
			XMVECTOR s = XMVectorSubtract(Origin, V0);

			// u = s * p;
			u = XMVector3Dot(s, p);

			XMVECTOR NoIntersection = XMVectorGreater(u, Zero);
			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(u, det));

			// q = s ^ e1;
			XMVECTOR q = XMVector3Cross(s, e1);

			// v = Direction * q;
			v = XMVector3Dot(Direction, q);

			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(v, Zero));
			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(XMVectorAdd(u, v), det));

			// t = e2 * q;
			t = XMVector3Dot(e2, q);

			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(t, Zero));

			if (XMVector4EqualInt(NoIntersection, XMVectorTrueInt()))
			{
				Dist = 0.f;
				return false;
			}
		}
		else
		{
			// Parallel ray.
			Dist = 0.f;
			return false;
		}

		t = XMVectorDivide(t, det);

		// Store the x-component to *pDist
		XMStoreFloat(&Dist, t);

		if (Dist > TMax || Dist < TMin)
			return false;

		return true;
	}
}