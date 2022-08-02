#include "gizmo.h"
#include "worldEditor.h"
#include "math\math.hpp"
#include "math\vMath_impl.hpp"

namespace VulkanTest
{
namespace Editor::Gizmo
{
	enum class Axis : U32
	{
		NONE,
		X,
		Y,
		Z,
		XY,
		XZ,
		YZ,
		XYZ
	};

	struct GizmoConstant
	{
		FMat4x4 pos;
		F32x4 col;
	};

	struct Vertex
	{
		F32x4 position;
		F32x4 color;
	};

	const F32 originSize = 0.2f;
	const U32 segmentCount = 18;
	const U32 cylinderTriangleCount = segmentCount * 2;
	const U32 coneTriangleCount = cylinderTriangleCount;
	const F32 coneLength = 0.75f;
	const F32 axisLength = 3.5f;
	const F32 cylinder_length = axisLength - coneLength;
	const F32 circleRadius = axisLength;
	const F32 circleWidth = 1;
	const F32 circle2Radius = circleRadius + 0.7f;
	const F32 circle2Width = 0.3f;
	const F32 planeMin = 0.5f;
	const F32 planeMax = 1.5f;
	const F32x4 highlightColor = F32x4(1.0f, 0.6f, 0.0f, 1.0f);

	struct
	{
		ECS::EntityID selectedID = ECS::INVALID_ENTITY;
		ECS::EntityID activeID = ECS::INVALID_ENTITY;
		ECS::EntityID draggedID = ECS::INVALID_ENTITY;
		Axis axis = Axis::NONE;
		F32x3 prevPoint = F32x3(0.0f);
		Transform transform;
		Transform prevTransform;
		F32 startAngle = 0.0f;
		F32 currentAngle = 0.0f;
	} 
	impl;

	Axis Collide(const Transform& transform, WorldView& view, const Gizmo::Config& cfg)
	{
		const CameraComponent& camera = view.GetCamera();
		F32x2 mousePos = view.GetMousePos();
		Ray ray = Renderer::GetPickRay(mousePos, camera);
		F32x3 position = transform.GetPosition();
		F32 dist = std::max(Distance(position, camera.eye) * 0.05f, 0.0001f);

		// Translate
		if (cfg.mode == Config::Mode::TRANSLATE)
		{
			AABB aabbOrigin = AABB::CreateFromHalfWidth(position, F32x3(originSize * dist, originSize * dist, originSize * dist));
			F32x3 maxp = position + F32x3(axisLength, 0, 0) * dist;
			AABB aabbX = AABB::Merge(AABB(Min(position, maxp), Max(position, maxp)), aabbOrigin);
			maxp = position + F32x3(0, axisLength, 0) * dist;
			AABB aabbY = AABB::Merge(AABB(Min(position, maxp), Max(position, maxp)), aabbOrigin);
			maxp = position + F32x3(0, 0, axisLength) * dist;
			AABB aabbZ = AABB::Merge(AABB(Min(position, maxp), Max(position, maxp)), aabbOrigin);

			if (aabbX.Intersects(ray))
				return Axis::X;
			else if (aabbY.Intersects(ray))
				return Axis::Y;
			else if (aabbZ.Intersects(ray))
				return Axis::Z;

			F32x3 minp = position + F32x3(planeMin, planeMin, 0.0f) * dist;
			maxp = position + F32x3(planeMax, planeMax, 0.0f) * dist;
			AABB aabbXY = AABB(Min(minp, maxp), Max(minp, maxp));

			minp = position + F32x3(planeMin, 0.0f, planeMin) * dist;
			maxp = position + F32x3(planeMax, 0.0f, planeMax) * dist;
			AABB aabbXZ = AABB(Min(minp, maxp), Max(minp, maxp));

			minp = position + F32x3(0.0f, planeMin, planeMin) * dist;
			maxp = position + F32x3(0.0f, planeMax, planeMax) * dist;
			AABB aabbYZ = AABB(Min(minp, maxp), Max(minp, maxp));

			// Find the closest plane (by checking plane ray trace distance):
			VECTOR rayDir = LoadF32x3(ray.direction);
			VECTOR posDelta = LoadF32x3(ray.origin - position);
			VECTOR N = VectorSet(0, 0, 1, 0);

			Axis ret = Axis::NONE;
			F32 prio = FLT_MAX;
			if (aabbXY.Intersects(ray))
			{
				ret = Axis::XY;
				prio = VectorGetX(Vector3Dot(N, VectorDivide(posDelta, VectorAbs(Vector3Dot(N, rayDir)))));
			}

			N = VectorSet(0, 1, 0, 0);
			float d = VectorGetX(Vector3Dot(N, VectorDivide(posDelta, VectorAbs(Vector3Dot(N, rayDir)))));
			if (d < prio && aabbXZ.Intersects(ray))
			{
				ret = Axis::XZ;
				prio = d;
			}

			N = VectorSet(1, 0, 0, 0);
			d = VectorGetX(Vector3Dot(N, VectorDivide(posDelta, VectorAbs(Vector3Dot(N, rayDir)))));
			if (d < prio && aabbYZ.Intersects(ray))
			{
				ret = Axis::YZ;
			}
			return ret;
		}
		else if (cfg.mode == Config::Mode::ROTATE)
		{
			VECTOR rayOrigin = LoadF32x3(ray.origin);
			VECTOR posV = LoadF32x3(position);
			VECTOR planeZY = PlaneFromPointNormal(posV, VectorSet(1, 0, 0, 0));
			VECTOR planeXZ = PlaneFromPointNormal(posV, VectorSet(0, 1, 0, 0));
			VECTOR planeXY = PlaneFromPointNormal(posV, VectorSet(0, 0, 1, 0));
			VECTOR linePoint2 = LoadF32x3(ray.origin + ray.direction * camera.farZ);

			XMVECTOR intersection = PlaneIntersectLine(planeZY, rayOrigin, linePoint2);
			float dist_x = XMVectorGetX(Vector3LengthSq(XMVectorSubtract(intersection, rayOrigin)));
			float len_x = VectorGetX(Vector3Length(XMVectorSubtract(intersection, posV))) / dist;
			intersection = PlaneIntersectLine(planeXZ, rayOrigin, linePoint2);
			float dist_y = VectorGetX(Vector3LengthSq(XMVectorSubtract(intersection, rayOrigin)));
			float len_y = VectorGetX(Vector3Length(XMVectorSubtract(intersection, posV))) / dist;
			intersection = PlaneIntersectLine(planeXY, rayOrigin, linePoint2);
			float dist_z = VectorGetX(Vector3LengthSq(XMVectorSubtract(intersection, rayOrigin)));
			float len_z = VectorGetX(Vector3Length(XMVectorSubtract(intersection, posV))) / dist;

			Axis ret = Axis::NONE;
			float range = circleWidth * 0.5f;
			float perimeter = circleRadius - range;
			float best_dist = std::numeric_limits<float>::max();
			if (std::abs(perimeter - len_x) <= range && dist_x < best_dist)
			{
				ret = Axis::X;
				best_dist = dist_x;
			}
			if (std::abs(perimeter - len_y) <= range && dist_y < best_dist)
			{
				ret = Axis::Y;
				best_dist = dist_y;
			}
			if (std::abs(perimeter - len_z) <= range && dist_z < best_dist)
			{
				ret = Axis::Z;
				best_dist = dist_z;
			}
			return ret;
		}

		return Axis::NONE;
	}

	F32x3 GetRotatorAxis(Axis axis)
	{
		switch (axis)
		{
		case Axis::X:
			return F32x3(1.0f, 0.0f, 0.0f);
		case Axis::Y:
			return F32x3(0.0f, 1.0f, 0.0f);
		case Axis::Z:
			return F32x3(0.0f, 0.0f, 1.0f);
		default:
			return F32x3(1.0f, 0.0f, 0.0f);
		}
	}

	F32x3 GetMouseRotatorIntersection(const Transform& transform, WorldView& view, Axis axis)
	{
		const CameraComponent& camera = view.GetCamera();
		F32x2 mousePos = view.GetMousePos();
		Ray ray = Renderer::GetPickRay(mousePos, camera);
		VECTOR pos = LoadF32x3(transform.GetPosition());
		VECTOR axisV = LoadF32x3(GetRotatorAxis(axis));
		return StoreF32x3(PlaneIntersectLine(XMPlaneFromPointNormal(pos, axisV), LoadF32x3(ray.origin), LoadF32x3(ray.origin + ray.direction * camera.farZ)));
	}

	F32x3 GetMousePlaneIntersection(const Transform& transform, WorldView& view, Axis axis)
	{
		const CameraComponent& camera = view.GetCamera();
		F32x2 mousePos = view.GetMousePos();
		Ray ray = Renderer::GetPickRay(mousePos, camera);
		VECTOR cameraAt = LoadF32x3(camera.at);
		VECTOR cameraUp = LoadF32x3(camera.up);

		// Get intersecting plane according the axis
		VECTOR planeNormal;
		if (axis == Axis::X)
		{
			VECTOR axis = VectorSet(1, 0, 0, 0);
			VECTOR wrong = Vector3Cross(cameraAt, axis);
			planeNormal = Vector3Cross(wrong, axis);
		}
		else if (axis == Axis::Y)
		{
			VECTOR axis = VectorSet(0, 1, 0, 0);
			VECTOR wrong = Vector3Cross(cameraAt, axis);
			planeNormal = Vector3Cross(wrong, axis);
		}
		else if (axis == Axis::Z)
		{
			VECTOR axis = VectorSet(0, 0, 1, 0);
			VECTOR wrong = Vector3Cross(cameraUp, axis);
			planeNormal = Vector3Cross(wrong, axis);
		}
		else if (axis == Axis::XY)
		{
			planeNormal = VectorSet(0, 0, 1, 0);
		}
		else if (axis == Axis::XZ)
		{
			planeNormal = VectorSet(0, 1, 0, 0);
		}
		else if (axis == Axis::YZ)
		{
			planeNormal = VectorSet(1, 0, 0, 0);
		}
		else
		{
			// xyz
			planeNormal = cameraAt;
		}

		VECTOR pos = LoadF32x3(transform.GetPosition());
		VECTOR plane = PlaneFromPointNormal(pos, Vector3Normalize(planeNormal));
		F32x3 ret = StoreF32x3(PlaneIntersectLine(plane, LoadF32x3(ray.origin), LoadF32x3(ray.origin + ray.direction * camera.farZ)));
		switch (axis)
		{
		case Axis::X:
			ret.y = ret.z = 0.0f;
			break;
		case Axis::Y:
			ret.x = ret.z = 0.0f;
			break;
		case Axis::Z:
			ret.x = ret.y = 0.0f;
			break;
		default:
			break;
		}
		return ret;
	}

	bool DoTranslate(ECS::EntityID entity, Transform& transform, WorldView& view, const Config& config)
	{
		const bool noneActive = impl.draggedID == ECS::INVALID_ENTITY;
		if (noneActive)
		{
			const Axis axis = Collide(transform, view, config);
			if (axis != Axis::NONE)
				impl.activeID = entity;
			else if (impl.activeID != ECS::INVALID_ENTITY)
				impl.activeID = ECS::INVALID_ENTITY;

			if (view.IsMouseClick(Platform::MouseButton::LEFT) && axis != Axis::NONE)
			{
				impl.draggedID = entity;
				impl.axis = axis;
				impl.prevTransform = transform;
				impl.prevPoint = GetMousePlaneIntersection(transform, view, axis);
			}

			return false;
		}

		if (!view.IsMouseDown(Platform::MouseButton::LEFT))
		{
			impl.draggedID = ECS::INVALID_ENTITY;
			impl.axis = Axis::NONE;
			return false;
		}

		const F32x3 pos = GetMousePlaneIntersection(transform, view, impl.axis);
		const F32x3 delta = pos - impl.prevPoint;
		F32x3 ret = transform.translation + delta;
		impl.prevPoint = pos;
		transform.translation = ret;
		return SquaredLength(ret) > 0.0f;
	}

	bool DoRotate(ECS::EntityID entity, Transform& transform, WorldView& view, const Config& config)
	{
		const bool noneActive = impl.draggedID == ECS::INVALID_ENTITY;
		if (noneActive)
		{
			const Axis axis = Collide(transform, view, config);
			if (axis != Axis::NONE)
				impl.activeID = entity;
			else if (impl.activeID != ECS::INVALID_ENTITY)
				impl.activeID = ECS::INVALID_ENTITY;

			if (view.IsMouseClick(Platform::MouseButton::LEFT) && axis != Axis::NONE)
			{
				impl.draggedID = entity;
				impl.axis = axis;
				impl.prevTransform = transform;
				impl.prevPoint = GetMouseRotatorIntersection(transform, view, axis);
			}

			return false;
		}

		if (!view.IsMouseDown(Platform::MouseButton::LEFT))
		{
			impl.draggedID = ECS::INVALID_ENTITY;
			impl.axis = Axis::NONE;
			return false;
		}

		// Compute the angle
		const F32x3 point = GetMouseRotatorIntersection(transform, view, impl.axis);
		XMVECTOR o = Vector3Normalize(LoadF32x3(impl.prevPoint - transform.GetPosition()));
		XMVECTOR c = Vector3Normalize(LoadF32x3(point - transform.GetPosition()));
		F32x3 original = StoreF32x3(o);
		F32x3 current = StoreF32x3(c);
		F32x3 axis = GetRotatorAxis(impl.axis);
		F32 angle = GetAngle(original, current, axis);
		angle = std::fmod(angle, MATH_2PI);

		switch (impl.axis)
		{
		case Axis::X:
			impl.currentAngle = GetAngle(F32x3(0.0f, 1.0f, 0.0f), original, axis);
			break;
		case Axis::Y:
			impl.currentAngle = GetAngle(F32x3(0.0f, 0.0f, 1.0f), original, axis);
			break;
		case Axis::Z:
			impl.currentAngle = GetAngle(F32x3(1.0f, 0.0f, 0.0f), original, axis);
			break;
		default:
			break;
		}
		impl.currentAngle = angle;

		transform = impl.prevTransform;
		transform.Rotate(StoreF32x4(XMQuaternionRotationAxis(LoadF32x3(axis), angle)));

		return true;
	}

	bool DoScale(ECS::EntityID entity, Transform& transform, WorldView& view, const Config& config)
	{
		return false;
	}

	void Update()
	{
		impl.selectedID = ECS::INVALID_ENTITY;
	}

	bool Manipulate(ECS::EntityID entity, Transform& transform, WorldView& view, const Config& config)
	{
		if (!config.enable)
			return false;

		impl.selectedID = entity;
		impl.transform = transform;

		switch (config.mode)
		{
		case Config::Mode::TRANSLATE:
			return DoTranslate(entity, transform, view, config);
		case Config::Mode::ROTATE:
			return DoRotate(entity, transform, view, config);
		case Config::Mode::SCALE:
			return DoScale(entity, transform, view, config);
		default:
			ASSERT(false);
			return false;
		}
	}

	void SetupTranslatorVertexData(U8* dst, size_t size)
	{
		const F32x4 color = F32x4(1.0f);
		U8* old = dst;
		for (uint32_t i = 0; i < segmentCount; ++i)
		{
			const F32 angle0 = (F32)i / (F32)segmentCount * MATH_2PI;
			const F32 angle1 = (F32)(i + 1) / (F32)segmentCount * MATH_2PI;
			// Cylinder base:
			{
				const float cylinder_radius = 0.075f;
				const Vertex verts[] = {
					{F32x4(originSize, std::sin(angle0) * cylinder_radius, std::cos(angle0) * cylinder_radius, 1), color},
					{F32x4(originSize, std::sin(angle1) * cylinder_radius, std::cos(angle1) * cylinder_radius, 1), color},
					{F32x4(cylinder_length, std::sin(angle0) * cylinder_radius, std::cos(angle0) * cylinder_radius, 1), color},
					{F32x4(cylinder_length, std::sin(angle0) * cylinder_radius, std::cos(angle0) * cylinder_radius, 1), color},
					{F32x4(cylinder_length, std::sin(angle1) * cylinder_radius, std::cos(angle1) * cylinder_radius, 1), color},
					{F32x4(originSize, std::sin(angle1) * cylinder_radius, std::cos(angle1) * cylinder_radius, 1), color},
				};
				std::memcpy(dst, verts, sizeof(verts));
				dst += sizeof(verts);
			}

			// cone cap:
			{
				const float cone_radius = originSize;
				const Vertex verts[] = {
					{F32x4(cylinder_length, 0, 0, 1), color},
					{F32x4(cylinder_length, std::sin(angle0) * cone_radius, std::cos(angle0) * cone_radius, 1), color},
					{F32x4(cylinder_length, std::sin(angle1) * cone_radius, std::cos(angle1) * cone_radius, 1), color},
					{F32x4(axisLength, 0, 0, 1), color},
					{F32x4(cylinder_length, std::sin(angle0) * cone_radius, std::cos(angle0) * cone_radius, 1), color},
					{F32x4(cylinder_length, std::sin(angle1) * cone_radius, std::cos(angle1) * cone_radius, 1), color},
				};
				std::memcpy(dst, verts, sizeof(verts));
				dst += sizeof(verts);
			}
		}

		ASSERT(size == (dst - old));
	}

	void DrawTranslator(GPU::CommandList& cmd, CameraComponent& camera, const Transform& transform, const Config& config)
	{
		U32 vertexCount = 0;
		vertexCount = (cylinderTriangleCount + coneTriangleCount) * 3;
		Vertex* vertMem = static_cast<Vertex*>(cmd.AllocateVertexBuffer(0, sizeof(Vertex) * vertexCount, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX));
		SetupTranslatorVertexData((U8*)vertMem, sizeof(Vertex) * vertexCount);
		cmd.SetVertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
		cmd.SetVertexAttribute(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(F32x4));
		cmd.SetDefaultTransparentState();
		cmd.SetBlendState(Renderer::GetBlendState(BSTYPE_TRANSPARENT));
		cmd.SetRasterizerState(Renderer::GetRasterizerState(RSTYPE_DOUBLE_SIDED));
		cmd.SetDepthStencilState(Renderer::GetDepthStencilState(DSTYPE_DEFAULT));
		cmd.SetProgram(
			Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetVS("VS"),
			Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetPS("PS")
		);

		F32 dist = std::max(Distance(transform.GetPosition(), camera.eye) * 0.05f, 0.0001f);
		MATRIX mat = MatrixScaling(dist, dist, dist) * MatrixTranslationFromVector(LoadF32x3(transform.GetPosition())) * camera.GetViewProjection();

		// X
		GizmoConstant constant = {};
		constant.pos = StoreFMat4x4(MatrixIdentity() * mat);
		constant.col = impl.axis == Axis::X ? highlightColor : F32x4(1.0f, 0.25f, 0.25f, 1.0f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(vertexCount);

		// Y
		constant.pos = StoreFMat4x4(MatrixRotationZ(MATH_PIDIV2) * MatrixRotationY(MATH_PIDIV2) * mat);
		constant.col = impl.axis == Axis::Y ? highlightColor : F32x4(0.25f, 1.0f, 0.25f, 1.0f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(vertexCount);

		// Z
		constant.pos = StoreFMat4x4(MatrixRotationY(-MATH_PIDIV2) * MatrixRotationZ(-MATH_PIDIV2) * mat);
		constant.col = impl.axis == Axis::Z ? highlightColor : F32x4(0.25f, 0.25f, 1.0f, 1.0f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(vertexCount);

		const F32x4 color = F32x4(1.0f);
		const Vertex verts[] = {
			{F32x4(planeMin, planeMin, 0.0f, 1.0f), color},
			{F32x4(planeMax, planeMin, 0.0f, 1.0f), color},
			{F32x4(planeMax, planeMax, 0.0f, 1.0f), color},

			{F32x4(planeMin, planeMin, 0.0f, 1.0f), color},
			{F32x4(planeMax, planeMax, 0.0f, 1.0f), color},
			{F32x4(planeMin, planeMax, 0.0f, 1.0f), color},
		};
		vertMem = static_cast<Vertex*>(cmd.AllocateVertexBuffer(0, sizeof(Vertex) * ARRAYSIZE(verts), sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX));
		std::memcpy(vertMem, verts, sizeof(verts));

		// XY
		constant.pos = StoreFMat4x4(MatrixIdentity() * mat);
		constant.col = impl.axis == Axis::XY ? highlightColor : F32x4(planeMin, planeMin, planeMax, 0.4f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(ARRAYSIZE(verts));

		// XZ
		constant.pos = StoreFMat4x4(MatrixRotationY(-MATH_PIDIV2) * MatrixRotationZ(-MATH_PIDIV2) * mat);
		constant.col = impl.axis == Axis::XZ ? highlightColor : F32x4(planeMin, planeMax, planeMin, 0.4f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(ARRAYSIZE(verts));

		// YZ
		constant.pos = StoreFMat4x4(MatrixRotationZ(MATH_PIDIV2) * MatrixRotationY(MATH_PIDIV2) * mat);
		constant.col = impl.axis == Axis::YZ ? highlightColor : F32x4(planeMax, planeMin, planeMin, 0.4f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(ARRAYSIZE(verts));
	}

	void DrawRotator(GPU::CommandList& cmd, CameraComponent& camera, const Transform& transform, const Config& config)
	{
		const F32x4 color = F32x4(1.0f);
		const uint32_t segmentCount = 90;
		const uint32_t circleTriangleCount = segmentCount * 2;
		U32 vertexCount = circleTriangleCount * 3;
		Vertex* vertMem = static_cast<Vertex*>(cmd.AllocateVertexBuffer(0, sizeof(Vertex) * vertexCount, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX));
		for (U32 i = 0; i < segmentCount; ++i)
		{
			const F32 angle0 = (F32)i / (F32)segmentCount * MATH_2PI;
			const F32 angle1 = (F32)(i + 1) / (F32)segmentCount * MATH_2PI;

			const F32 circleRadiusInner = circleRadius - circleWidth;
			const Vertex verts[] = {
				{F32x4(0, std::sin(angle0) * circleRadiusInner, std::cos(angle0) * circleRadiusInner, 1), color},
				{F32x4(0, std::sin(angle1) * circleRadiusInner, std::cos(angle1) * circleRadiusInner, 1), color},
				{F32x4(0, std::sin(angle0) * circleRadius, std::cos(angle0) * circleRadius, 1), color},
				{F32x4(0, std::sin(angle0) * circleRadius, std::cos(angle0) * circleRadius, 1), color},
				{F32x4(0, std::sin(angle1) * circleRadius, std::cos(angle1) * circleRadius, 1), color},
				{F32x4(0, std::sin(angle1) * circleRadiusInner, std::cos(angle1) * circleRadiusInner, 1), color},
			};
			std::memcpy(vertMem, verts, sizeof(verts));
			vertMem += ARRAYSIZE(verts);
		}

		cmd.SetVertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
		cmd.SetVertexAttribute(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(F32x4));
		cmd.SetDefaultTransparentState();
		cmd.SetBlendState(Renderer::GetBlendState(BSTYPE_TRANSPARENT));
		cmd.SetRasterizerState(Renderer::GetRasterizerState(RSTYPE_DOUBLE_SIDED));
		cmd.SetDepthStencilState(Renderer::GetDepthStencilState(DSTYPE_DEFAULT));
		cmd.SetProgram(
			Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetVS("VS"),
			Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetPS("PS")
		);

		F32 dist = std::max(Distance(transform.GetPosition(), camera.eye) * 0.05f, 0.0001f);
		MATRIX mat = MatrixScaling(dist, dist, dist) * MatrixTranslationFromVector(LoadF32x3(transform.GetPosition())) * camera.GetViewProjection();

		// Draw circles base on axises
		// X
		GizmoConstant constant = {};
		constant.pos = StoreFMat4x4(MatrixIdentity() * mat);
		constant.col = impl.axis == Axis::X ? highlightColor : F32x4(1.0f, 0.25f, 0.25f, 1.0f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(vertexCount);

		// Y
		constant.pos = StoreFMat4x4(MatrixRotationZ(MATH_PIDIV2) * MatrixRotationY(MATH_PIDIV2) * mat);
		constant.col = impl.axis == Axis::Y ? highlightColor : F32x4(0.25f, 1.0f, 0.25f, 1.0f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(vertexCount);

		// Z
		constant.pos = StoreFMat4x4(MatrixRotationY(-MATH_PIDIV2) * MatrixRotationZ(-MATH_PIDIV2) * mat);
		constant.col = impl.axis == Axis::Z ? highlightColor : F32x4(0.25f, 0.25f, 1.0f, 1.0f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(vertexCount);

		// Draw an other circle for rotator, a bit thinner and screen facing
#if 0
		{
			const uint32_t segmentCount = 90;
			const uint32_t circleTriangleCount = segmentCount * 2;
			U32 vertexCount = circleTriangleCount * 3;
			Vertex* vertMem = static_cast<Vertex*>(cmd.AllocateVertexBuffer(0, sizeof(Vertex) * vertexCount, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX));
			for (U32 i = 0; i < segmentCount; ++i)
			{
				const F32 angle0 = (F32)i / (F32)segmentCount * MATH_2PI;
				const F32 angle1 = (F32)(i + 1) / (F32)segmentCount * MATH_2PI;

				const float circle2RadiusInner = circle2Radius - circle2Width;
				const Vertex verts[] = {
					{F32x4(0, std::sin(angle0) * circle2RadiusInner, std::cos(angle0) * circle2RadiusInner, 1), color},
					{F32x4(0, std::sin(angle1) * circle2RadiusInner, std::cos(angle1) * circle2RadiusInner, 1), color},
					{F32x4(0, std::sin(angle0) * circle2Radius, std::cos(angle0) * circle2Radius, 1), color},
					{F32x4(0, std::sin(angle0) * circle2Radius, std::cos(angle0) * circle2Radius, 1), color},
					{F32x4(0, std::sin(angle1) * circle2Radius, std::cos(angle1) * circle2Radius, 1), color},
					{F32x4(0, std::sin(angle1) * circle2RadiusInner, std::cos(angle1) * circle2RadiusInner, 1), color},
				};
				std::memcpy(vertMem, verts, sizeof(verts));
				vertMem += ARRAYSIZE(verts);
			}

			// Calculate inversed matrix to face screen
			auto invMat = MatrixInverse(MatrixLookToLH(VectorZero(), Vector3Normalize(LoadF32x3(transform.GetPosition() - camera.eye)), LoadF32x3(camera.up)));
			constant.pos = StoreFMat4x4(MatrixRotationY(MATH_PIDIV2) * invMat * mat);
			constant.col = impl.axis == Axis::XYZ ? highlightColor : F32x4(1.0f, 1.0f, 1.0f, 0.5f);
			cmd.BindConstant<GizmoConstant>(constant, 0, 0);
			cmd.Draw(vertexCount);
		}
#endif

		// Draw dragging
		if (impl.draggedID != ECS::INVALID_ENTITY)
		{
			const F32 angle = impl.currentAngle;
			const F32 prevAngle = impl.startAngle;
			const uint32_t segmentCount = 90;
			const uint32_t vertexCount = segmentCount * 3;
			Vertex* vertMem = static_cast<Vertex*>(cmd.AllocateVertexBuffer(0, sizeof(Vertex) * vertexCount, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX));
			for (U32 i = 0; i < segmentCount; ++i)
			{
				const float angle0 = (float)i / (float)segmentCount * angle + prevAngle;
				const float angle1 = (float)(i + 1) / (float)segmentCount * angle + prevAngle;

				const float radius = (circleRadius - circleWidth);
				const Vertex verts[] = {
					{F32x4(0, 0, 0, 1), color},
					{F32x4(0, std::cos(angle0) * radius, std::sin(angle0) * radius, 1), color},
					{F32x4(0, std::cos(angle1) * radius, std::sin(angle1) * radius, 1), color},
				};
				std::memcpy(vertMem, verts, sizeof(verts));
				vertMem += ARRAYSIZE(verts);
			}

			switch (impl.axis)
			{
			case Axis::X:
				constant.pos = StoreFMat4x4(MatrixIdentity() * mat);
				break;
			case Axis::Y:
				constant.pos = StoreFMat4x4(MatrixRotationZ(MATH_PIDIV2) * MatrixRotationY(MATH_PIDIV2) * mat);
				break;
			case Axis::Z:
				constant.pos = StoreFMat4x4(MatrixRotationY(-MATH_PIDIV2) * MatrixRotationZ(-MATH_PIDIV2) * mat);
				break;
			default:
				break;
			}
			constant.col = F32x4(1.0f, 1.0f, 1.0f, 0.8f);
			cmd.BindConstant<GizmoConstant>(constant, 0, 0);
			cmd.Draw(vertexCount);
		}
	}

	void DrawScaler(GPU::CommandList& cmd, CameraComponent& camera, const Transform& transform, const Config& config)
	{
	}

	void Draw(GPU::CommandList& cmd, CameraComponent& camera, const Config& config)
	{
		if (!config.enable)
			return;

		if (impl.selectedID == ECS::INVALID_ENTITY)
			return;

		switch (config.mode)
		{
		case Config::Mode::TRANSLATE:
			return DrawTranslator(cmd, camera, impl.transform, config);
		case Config::Mode::ROTATE:
			return DrawRotator(cmd, camera, impl.transform, config);
		case Config::Mode::SCALE:
			return DrawScaler(cmd, camera, impl.transform, config);
		default:
			ASSERT(false);
			return;
		}
	}
}
}