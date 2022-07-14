#include "gizmo.h"

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
	const U32 segmentCount = 8;
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

		GizmoConstant constant = {};
		constant.pos = IDENTITY_MATRIX;
		constant.col = F32x4(1.0f, 0.25f, 0.25f, 1.0f);
		GizmoConstant* mem = cmd.AllocateConstant<GizmoConstant>(0, 0);
		memcpy(mem, &constant, sizeof(GizmoConstant));

		cmd.SetDefaultTransparentState();
		cmd.SetBlendState(Renderer::GetBlendState(BSTYPE_TRANSPARENT));
		cmd.SetRasterizerState(Renderer::GetRasterizerState(RSTYPE_DOUBLE_SIDED));
		cmd.SetDepthStencilState(Renderer::GetDepthStencilState(DSTYPE_DEFAULT));
		cmd.SetProgram(
			Renderer::GetShader(ShaderType::SHADERTYPE_VS_VERTEXCOLOR),
			Renderer::GetShader(ShaderType::SHADERTYPE_PS_VERTEXCOLOR)
		);
		cmd.Draw(vertexCount);
	}
}
}