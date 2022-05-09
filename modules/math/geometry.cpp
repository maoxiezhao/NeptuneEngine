#include "geometry.h"

namespace VulkanTest
{
	void AABB::Merge(const AABB& rhs)
	{
		AddPoint(rhs.min);
		AddPoint(rhs.max);
	}

	void AABB::AddPoint(const F32x3& point)
	{
		min = Min(min, point);
		max = Max(max, point);
	}

	void Frustum::Compute(const MATRIX& viewProjection)
	{
		// p' = p T = (pC1, pC2, pC3, pC4)
		// (-1 < x'/w)  => (-w' <= x)'
		// 0 <= x' + w' => p(C1 + C4)
		const auto t = MatrixTranspose(viewProjection);
		// Near plane:
		planes[4] = StoreF32x4(PlaneNormalize(t.r[2]));
		// Far plane:
		planes[5] = StoreF32x4(PlaneNormalize(t.r[3] - t.r[2]));
		// Left plane:
		planes[0] = StoreF32x4(PlaneNormalize(t.r[3] + t.r[0]));
		// Right plane:
		planes[1] = StoreF32x4(PlaneNormalize(t.r[3] - t.r[0]));
		// Top plane:
		planes[2] = StoreF32x4(PlaneNormalize(t.r[3] - t.r[1]));
		// Bottom plane:
		planes[3] = StoreF32x4(PlaneNormalize(t.r[3] + t.r[1]));
	}

	bool Frustum::CheckBoxFast(const AABB& box) const
	{
		VECTOR max = LoadF32x3(box.max);
		VECTOR min = LoadF32x3(box.min);
		VECTOR zero = VectorZero();
		for (size_t p = 0; p < 6; ++p)
		{
			VECTOR plane = LoadF32x4(planes[p]);
			VECTOR lt = VectorLess(plane, zero);
			VECTOR furthestFromPlane = VectorSelect(max, min, lt);
			if (VectorGetX(PlaneDotCoord(plane, furthestFromPlane)) < 0.0f)
				return false;
		}
		return true;
	}
}