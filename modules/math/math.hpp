#pragma once

#include "math_common.h"
#include "vMath.h"

#ifdef USE_DIRECTX_MATH
#include "directXMath.h"
#endif

namespace VulkanTest
{
	constexpr F32x3 Max(const F32x3& a, const F32x3& b) {
		return F32x3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
	}

	constexpr F32x3 Min(const F32x3& a, const F32x3& b) {
		return F32x3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
	}
}