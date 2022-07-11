#pragma once

#include "math.hpp"

namespace VulkanTest
{
	struct AABB
	{
		F32x3 min;
		F32x3 max;
	
		AABB(
			const F32x3& _min = F32x3(std::numeric_limits<F32>::max(), std::numeric_limits<F32>::max(), std::numeric_limits<F32>::max()),
			const F32x3& _max = F32x3(std::numeric_limits<F32>::lowest(), std::numeric_limits<F32>::lowest(), std::numeric_limits<F32>::lowest())
		) : min(_min), max(_max) 
		{}

		void Merge(const AABB& rhs);
		void AddPoint(const F32x3& point);
		AABB Transform(const MATRIX& mat) const;
		F32x3 GetCenter() const;
		F32x3 GetHalfWidth() const;
		MATRIX GetCenterAsMatrix() const;
	};

	struct Frustum
	{
		enum class Planes
		{
			Near,
			Far,
			Left,
			Right,
			Top,
			Bottom,
			Count
		};
		F32x4 planes[(U32)Planes::Count];

		void Compute(const MATRIX& viewProjection);

		bool CheckPoint(const F32x3&) const;
		bool CheckSphere(const F32x3&, float) const;

		enum BoxFrustumIntersect
		{
			BOX_FRUSTUM_OUTSIDE,
			BOX_FRUSTUM_INTERSECTS,
			BOX_FRUSTUM_INSIDE,
		};
		BoxFrustumIntersect CheckBox(const AABB& box) const;
		bool CheckBoxFast(const AABB& box) const;

		const F32x4& GetPlane(Planes plane) const
		{
			return planes[(U32)plane];
		}
	};
}