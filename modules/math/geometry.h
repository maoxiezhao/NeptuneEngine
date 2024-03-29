#pragma once

#include "math.hpp"

namespace VulkanTest
{
	struct Sphere
	{
	public:
		F32x3 center;
		F32 radius;

	public:
		Sphere() : center(F32x3(0.0f)), radius(0.0f) {}
		Sphere(const F32x3& center_, F32 radius_) : center(center_), radius(radius_) {}
	};

	struct Rectangle
	{
	public:
		F32x2 pos;
		F32x2 size;

		static Rectangle Empty;

	public:
		Rectangle()
		{
		}

		Rectangle(F32 x, F32 y, F32 width, F32 height)
			: pos(x, y)
			, size(width, height)
		{
		}

		Rectangle(const F32x2& pos_, const F32x2& size_)
			: pos(pos_)
			, size(size_)
		{
		}

		F32 GetWidth() const
		{
			return size.x;
		}

		F32 GetHeight() const
		{
			return size.y;
		}

		F32 GetY() const
		{
			return pos.y;
		}

		F32 GetTop() const
		{
			return pos.y;
		}

		F32 GetBottom() const
		{
			return pos.y + size.y;
		}

		F32 GetX() const
		{
			return pos.x;
		}

		F32 GetLeft() const
		{
			return pos.x;
		}

		F32 GetRight() const
		{
			return pos.x + size.x;
		}

		F32x2 GetUpperLeft() const
		{
			return pos;
		}

		F32x2 GetUpperRight() const
		{
			return F32x2(pos.x + size.x, pos.y);
		}

		F32x2 GetBottomRight() const
		{
			return F32x2(pos.x + size.x, pos.y +size.y);
		}

		F32x2 GetBottomLeft() const
		{
			return F32x2(pos.x, pos.y + size.y);
		}

		F32x2 GetCenter() const
		{
			return F32x2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
		}
	};

	struct Ray
	{
		F32x3 origin;
		F32x3 direction;
		F32 tMin = 0;
		F32 tMax = std::numeric_limits<float>::max();
		F32x3 directionInv;

		Ray(const F32x3& origin_ = F32x3(0.0f),
			const F32x3& direction_ = F32x3(0.0f, 0.0f, 1.0f),
			F32 tMin_ = 0, F32 tMax_ = std::numeric_limits<float>::max()) :
			origin(origin_),
			direction(direction_),
			tMin(tMin_),
			tMax(tMax_)
		{
			directionInv = StoreF32x3(VectorDivide(VectorReplicate(1.0f), LoadF32x3(direction_)));
		}
	
		bool Intersects(const struct AABB& b) const;
	};

	struct AABB
	{
		F32x3 min;
		F32x3 max;
	
		AABB(
			const F32x3& _min = F32x3(std::numeric_limits<F32>::max(), std::numeric_limits<F32>::max(), std::numeric_limits<F32>::max()),
			const F32x3& _max = F32x3(std::numeric_limits<F32>::lowest(), std::numeric_limits<F32>::lowest(), std::numeric_limits<F32>::lowest())
		) : min(_min), max(_max) 
		{}

		static AABB CreateFromHalfWidth(const F32x3& center, const F32x3& halfwidth);
		static AABB Merge(const AABB& a, const AABB& b);

		void FromHalfWidth(const F32x3& center, const F32x3& halfwidth);
		void Merge(const AABB& rhs);
		void AddPoint(const F32x3& point);
		AABB Transform(const MATRIX& mat) const;
		F32x3 GetCenter() const;
		F32x3 GetHalfWidth() const;
		F32 GetRadius()const;
		MATRIX GetAsMatrix() const;

		constexpr bool IsValid() const
		{
			return (min.x <= max.x && min.y <= max.y && min.z <= max.z);
		}

		bool Intersects(const F32x3& p) const;
		bool Intersects(const Ray& ray) const;
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