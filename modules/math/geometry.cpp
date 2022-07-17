#include "geometry.h"
#include "vMath_impl.hpp"

namespace VulkanTest
{
	bool Ray::Intersects(const AABB& b) const
	{
		return b.Intersects(*this);
	}

	AABB AABB::CreateFromHalfWidth(const F32x3& center, const F32x3& halfwidth)
	{
		return AABB(
			F32x3(center.x - halfwidth.x, center.y - halfwidth.y, center.z - halfwidth.z),
			F32x3(center.x + halfwidth.x, center.y + halfwidth.y, center.z + halfwidth.z)
		);
	}

	void AABB::Merge(const AABB& rhs)
	{
		AddPoint(rhs.min);
		AddPoint(rhs.max);
	}

	AABB AABB::Merge(const AABB& a, const AABB& b)
	{
		return AABB(Min(a.min, b.min), Max(a.max, b.max));
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

	bool AABB::Intersects(const F32x3& p) const
	{
		if (!IsValid())
			return false;
		return !(p.x > max.x || 
			     p.x < min.x ||
				 p.y > max.y ||
				 p.y < min.y ||
				 p.z > max.z ||
				 p.z < min.z);
	}

	bool AABB::Intersects(const Ray& ray) const
	{
		if (!IsValid())
			return false;

		if (Intersects(ray.origin))
			return true;

		F32 tx1 = (min.x - ray.origin.x) * ray.directionInv.x;
		F32 tx2 = (max.x - ray.origin.x) * ray.directionInv.x;
		F32 tmin = std::min(tx1, tx2);
		F32 tmax = std::max(tx1, tx2);
		if (ray.tMax < tmin || ray.tMin > tmax)
			return false;

		F32 ty1 = (min.y - ray.origin.y) * ray.directionInv.y;
		F32 ty2 = (max.y - ray.origin.y) * ray.directionInv.y;
		tmin = std::max(tmin, std::min(ty1, ty2));
		tmax = std::min(tmax, std::max(ty1, ty2));
		if (ray.tMax < tmin || ray.tMin > tmax)
			return false;

		F32 tz1 = (min.z - ray.origin.z) * ray.directionInv.z;
		F32 tz2 = (max.z - ray.origin.z) * ray.directionInv.z;
		tmin = std::max(tmin, std::min(tz1, tz2));
		tmax = std::min(tmax, std::max(tz1, tz2));
		if (ray.tMax < tmin || ray.tMin > tmax)
			return false;

		return tmax >= tmin;
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