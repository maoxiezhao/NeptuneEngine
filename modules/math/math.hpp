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

	struct Transform
	{
		F32x3 translation = F32x3(0.f, 0.f, 0.f);
		F32x4 rotation = F32x4(0.f, 0.f, 0.f, 1.f);	// quaternion
		F32x3 scale = F32x3(1.f, 1.f, 1.f);
		FMat4x4 world = IDENTITY_MATRIX;
		bool isDirty = false;

		void UpdateTransform();
		MATRIX GetMatrix()const;

		void Translate(const F32x3& value);
		void RotateRollPitchYaw(const F32x3& value);
		void Rotate(const F32x4& quaternion);
		void Scale(const F32x3& value);

		F32x3 DoTransform(const F32x3& value) const;
		F32x4 DoTransform(const F32x4& value) const;
	};
}