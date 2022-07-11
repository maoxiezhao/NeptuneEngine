#include "geometry.h"
#include "vMath_impl.hpp"

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

	AABB AABB::Transform(const MATRIX& mat) const
	{
		const XMVECTOR vcorners[8] = {
			Vector3Transform(LoadF32x3(min), mat),
			Vector3Transform(VectorSet(min.x, max.y, min.z, 1), mat),
			Vector3Transform(VectorSet(min.x, max.y, max.z, 1), mat),
			Vector3Transform(VectorSet(min.x, min.y, max.z, 1), mat),
			Vector3Transform(VectorSet(max.x, min.y, min.z, 1), mat),
			Vector3Transform(VectorSet(max.x, max.y, min.z, 1), mat),
			Vector3Transform(LoadF32x3(max), mat),
			Vector3Transform(VectorSet(max.x, min.y, max.z, 1), mat),
		};
		XMVECTOR vmin = vcorners[0];
		XMVECTOR vmax = vcorners[0];
		vmin = VectorMin(vmin, vcorners[1]);
		vmax = VectorMax(vmax, vcorners[1]);
		vmin = VectorMin(vmin, vcorners[2]);
		vmax = VectorMax(vmax, vcorners[2]);
		vmin = VectorMin(vmin, vcorners[3]);
		vmax = VectorMax(vmax, vcorners[3]);
		vmin = VectorMin(vmin, vcorners[4]);
		vmax = VectorMax(vmax, vcorners[4]);
		vmin = VectorMin(vmin, vcorners[5]);
		vmax = VectorMax(vmax, vcorners[5]);
		vmin = VectorMin(vmin, vcorners[6]);
		vmax = VectorMax(vmax, vcorners[6]);
		vmin = VectorMin(vmin, vcorners[7]);
		vmax = VectorMax(vmax, vcorners[7]);

		return AABB(StoreF32x3(vmin), StoreF32x3(vmax));
	}

	F32x3 AABB::GetCenter() const
	{
		return (min + max) * 0.5f;
	}

	F32x3 AABB::GetHalfWidth() const
	{
		F32x3 center = GetCenter();
		return F32x3(abs(max.x - center.x), abs(max.y - center.y), abs(max.z - center.z));
	}

	MATRIX AABB::GetCenterAsMatrix() const
	{
		F32x3 ext = GetHalfWidth();
		MATRIX sca = XMMatrixScaling(ext.x, ext.y, ext.z);
		F32x3 pos = GetCenter();
		XMMATRIX tra = MatrixTranslation(pos.x, pos.y, pos.z);
		return sca * tra;
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
		planes[5] = StoreF32x4(PlaneNormalize(VectorSubtract(t.r[3], t.r[2])));
		// Left plane:
		planes[0] = StoreF32x4(PlaneNormalize(VectorAdd(t.r[3], t.r[0])));
		// Right plane:
		planes[1] = StoreF32x4(PlaneNormalize(VectorSubtract(t.r[3], t.r[0])));
		// Top plane:
		planes[2] = StoreF32x4(PlaneNormalize(VectorSubtract(t.r[3], t.r[1])));
		// Bottom plane:
		planes[3] = StoreF32x4(PlaneNormalize(VectorAdd(t.r[3], t.r[1])));
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