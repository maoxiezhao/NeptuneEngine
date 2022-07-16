#include "gizmo.h"
#include "math\math.hpp"

namespace VulkanTest
{
namespace Editor::Gizmo
{
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

	const F32 origin_size = 0.2f;
	const U32 segmentCount = 18;
	const U32 cylinder_triangleCount = segmentCount * 2;
	const U32 cone_triangleCount = cylinder_triangleCount;

	void SetupTranslatorVertexData(U8* dst, size_t size)
	{
		const F32 cone_length = 0.75f;
		const F32 axisLength = 3.5f;
		const F32 cylinder_length = axisLength - cone_length;
		const F32x4 color = F32x4(1.0f);

		U8* old = dst;
		for (uint32_t i = 0; i < segmentCount; ++i)
		{
			const F32 angle0 = (F32)i / (F32)segmentCount * MATH_2PI;
			const F32 angle1 = (F32)(i + 1) / (F32)segmentCount * MATH_2PI;
			// cylinder base:
			{
				const float cylinder_radius = 0.075f;
				const Vertex verts[] = {
					{F32x4(origin_size, std::sin(angle0) * cylinder_radius, std::cos(angle0) * cylinder_radius, 1), color},
					{F32x4(origin_size, std::sin(angle1) * cylinder_radius, std::cos(angle1) * cylinder_radius, 1), color},
					{F32x4(cylinder_length, std::sin(angle0) * cylinder_radius, std::cos(angle0) * cylinder_radius, 1), color},
					{F32x4(cylinder_length, std::sin(angle0) * cylinder_radius, std::cos(angle0) * cylinder_radius, 1), color},
					{F32x4(cylinder_length, std::sin(angle1) * cylinder_radius, std::cos(angle1) * cylinder_radius, 1), color},
					{F32x4(origin_size, std::sin(angle1) * cylinder_radius, std::cos(angle1) * cylinder_radius, 1), color},
				};
				std::memcpy(dst, verts, sizeof(verts));
				dst += sizeof(verts);
			}
	
			// cone cap:
			{
				const float cone_radius = origin_size;
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

	void Draw(GPU::CommandList& cmd, CameraComponent& camera, const Config& config)
	{
		U32 vertexCount = 0;
		vertexCount = (cylinder_triangleCount + cone_triangleCount) * 3;
		Vertex* vertMem = static_cast<Vertex*>(cmd.AllocateVertexBuffer(0, sizeof(Vertex) * vertexCount, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX));
		SetupTranslatorVertexData((U8*)vertMem, sizeof(Vertex) * vertexCount);
		cmd.SetVertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
		cmd.SetVertexAttribute(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(F32x4));
		cmd.SetDefaultTransparentState();
		cmd.SetBlendState(Renderer::GetBlendState(BSTYPE_TRANSPARENT));
		cmd.SetRasterizerState(Renderer::GetRasterizerState(RSTYPE_DOUBLE_SIDED));
		cmd.SetDepthStencilState(Renderer::GetDepthStencilState(DSTYPE_DEFAULT));
		cmd.SetProgram(
			Renderer::GetShader(ShaderType::SHADERTYPE_VS_VERTEXCOLOR),
			Renderer::GetShader(ShaderType::SHADERTYPE_PS_VERTEXCOLOR)
		);

		Transform transform;

		F32 dist = std::max(Distance(transform.GetPosition(), camera.eye) * 0.05f, 0.0001f);
		MATRIX mat = MatrixScaling(dist, dist, dist) * MatrixTranslationFromVector(LoadF32x3(transform.GetPosition())) * camera.GetViewProjection();

		// X
		GizmoConstant constant = {};
		constant.pos = StoreFMat4x4(MatrixIdentity() * mat);
		constant.col = F32x4(1.0f, 0.25f, 0.25f, 1.0f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(vertexCount);

		// Y
		constant.pos = StoreFMat4x4(MatrixRotationZ(MATH_PIDIV2) * MatrixRotationY(MATH_PIDIV2) * mat);
		constant.col = F32x4(0.25f, 1.0f, 0.25f, 1.0f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(vertexCount);
	
		// Z
		constant.pos = StoreFMat4x4(MatrixRotationY(-MATH_PIDIV2) * MatrixRotationZ(-MATH_PIDIV2) * mat);
		constant.col = F32x4(0.25f, 0.25f, 1.0f, 1.0f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(vertexCount);
	

		const F32 planeMin = 0.25f;
		const F32 planeMax = 1.25f;
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
		constant.col = F32x4(0.25f, 0.25f, 1.0f, 0.4f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(ARRAYSIZE(verts));

		// XZ
		constant.pos = StoreFMat4x4(MatrixRotationY(-MATH_PIDIV2) * MatrixRotationZ(-MATH_PIDIV2) * mat);
		constant.col = F32x4(0.25f, 1.0f, 0.25f, 0.4f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(ARRAYSIZE(verts));

		// YZ
		constant.pos = StoreFMat4x4(MatrixRotationZ(MATH_PIDIV2) * MatrixRotationY(MATH_PIDIV2) * mat);
		constant.col = F32x4(1.0f, 0.25f, 0.25f, 0.4f);
		cmd.BindConstant<GizmoConstant>(constant, 0, 0);
		cmd.Draw(ARRAYSIZE(verts));

	}
}
}