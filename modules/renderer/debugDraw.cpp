#include "debugDraw.h"

namespace VulkanTest
{
namespace DebugDraw
{
	struct DebugConstant
	{
		FMat4x4 pos;
		F32x4 col;
	};

	struct RenderableLine
	{
		F32x3 start;
		F32x3 end;
		F32x4 color;
	};

	GPU::BufferPtr wireCubeVB;
	GPU::BufferPtr wireCubeIB;
	GPU::BufferPtr wireSphereVB;
	GPU::BufferPtr wireSphereIB;
	I32 wireSphereIndices = 0;

	GPU::PipelineStateDesc debugPSO;
	GPU::PipelineStateDesc debugLinePSO;

	struct DebugDrawData
	{
		Array<RenderableLine> renderableLines;
		Array<Pair<FMat4x4, F32x4>> renderableBoxes;
		Array<Pair<Sphere, F32x4>> renderableSpheres;

		void Release()
		{
			renderableLines.resize(0);
			renderableBoxes.resize(0);
			renderableSpheres.resize(0);
		}
	};
	DebugDrawData debugDrawData;

	class DebugDrawService : public RendererService
	{
	public:
		DebugDrawService() :
			RendererService("DebugDraw", -900)
		{}

		bool Init(Engine& engine) override
		{
			auto& resManager = engine.GetResourceManager();

			// Rasterizer states
			GPU::RasterizerState rs;
			rs.fillMode = GPU::FILL_WIREFRAME;
			rs.cullMode = VK_CULL_MODE_NONE;
			rs.frontCounterClockwise = true;
			rs.depthBias = 0;
			rs.depthBiasClamp = 0;
			rs.slopeScaledDepthBias = 0;
			rs.depthClipEnable = true;
			rs.multisampleEnable = false;
			rs.antialiasedLineEnable = true;
			rs.conservativeRasterizationEnable = false;

			GPU::PipelineStateDesc state = {};
			memset(&state, 0, sizeof(state));
			state.depthStencilState = Renderer::GetDepthStencilState(DSTYPE_READ);
			state.blendState = Renderer::GetBlendState(BSTYPE_TRANSPARENT);
			state.rasterizerState = rs;
			state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			debugPSO = state;

			state.depthStencilState = Renderer::GetDepthStencilState(DSTYPE_DISABLED);
			debugLinePSO = state;

			initialized = true;
			return true;
		}

		void Uninit() override
		{
			wireCubeVB.reset();
			wireCubeIB.reset();
			wireSphereVB.reset();
			wireSphereIB.reset();
			debugDrawData.Release();
			initialized = false;
		}
	};
	DebugDrawService DebugDrawServiceInstance;

	void SetupWireCubeResources()
	{
		if (!wireCubeVB)
		{
			auto device = GPU::GPUDevice::Instance;
			F32x4 min = F32x4(-1, -1, -1, 1);
			F32x4 max = F32x4(1, 1, 1, 1);
			F32x4 verts[] = {
				min,						F32x4(1,1,1,1),
				F32x4(min.x,max.y,min.z,1),	F32x4(1,1,1,1),
				F32x4(min.x,max.y,max.z,1),	F32x4(1,1,1,1),
				F32x4(min.x,min.y,max.z,1),	F32x4(1,1,1,1),
				F32x4(max.x,min.y,min.z,1),	F32x4(1,1,1,1),
				F32x4(max.x,max.y,min.z,1),	F32x4(1,1,1,1),
				max,						F32x4(1,1,1,1),
				F32x4(max.x,min.y,max.z,1),	F32x4(1,1,1,1),
			};
			U16 indices[] = {
				0,1,1,2,0,3,0,4,1,5,4,5,
				5,6,4,7,2,6,3,7,2,3,6,7
			};

			GPU::BufferCreateInfo info = {};
			info.domain = GPU::BufferDomain::Device;
			info.size = sizeof(verts);
			info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			wireCubeVB = device->CreateBuffer(info, verts);
			device->SetName(*wireCubeVB, "WireCubeVB");

			info.size = sizeof(indices);
			info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			wireCubeIB = device->CreateBuffer(info, indices);
			device->SetName(*wireCubeIB, "WireCubeIB");
		}
	}

	void SetupWireSphereResources()
	{
		if (!wireSphereVB)
		{
			auto device = GPU::GPUDevice::Instance;
			const I32 segments = 36;
			struct Vertex
			{
				F32x4 position;
				F32x4 color;
			};
			Array<Vertex> vertices;
			// XY
			for (I32 i = 0; i <= segments; i++)
			{
				const F32 angle = F32(i) / F32(segments) * MATH_2PI;
				vertices.push_back({ F32x4(std::sinf(angle), std::cosf(angle), 0, 1), F32x4(1.0f) });
			}
			// XZ
			for (I32 i = 0; i <= segments; i++)
			{
				const F32 angle = F32(i) / F32(segments) * MATH_2PI;
				vertices.push_back({ F32x4(std::sinf(angle), 0, std::cosf(angle), 1), F32x4(1.0f) });
			}
			// YZ
			for (I32 i = 0; i <= segments; i++)
			{
				const F32 angle = F32(i) / F32(segments) * MATH_2PI;
				vertices.push_back({ F32x4(0, std::sinf(angle), std::cosf(angle), 1), F32x4(1.0f) });
			}

			Array<U16> indices;
			for (I32 i = 0; i < segments; i++)
			{
				indices.push_back(U16(i));
				indices.push_back(U16(i + 1));
			}
			for (I32 i = 0; i < segments; i++)
			{
				indices.push_back(U16(i + (segments + 1)));
				indices.push_back(U16(i + (segments + 1) + 1));
			}
			for (I32 i = 0; i < segments; i++)
			{
				indices.push_back(U16(i + (segments + 1) * 2));
				indices.push_back(U16(i + (segments + 1) * 2 + 1));
			}

			GPU::BufferCreateInfo info = {};
			info.domain = GPU::BufferDomain::Device;
			info.size = vertices.size() * sizeof(Vertex);
			info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			wireSphereVB = device->CreateBuffer(info, vertices.data());
			device->SetName(*wireSphereVB, "WireSphereVB");

			info.size = indices.size() * sizeof(U16);
			info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			wireSphereIB = device->CreateBuffer(info, indices.data());
			device->SetName(*wireSphereIB, "WireSphereIB");
			wireSphereIndices = indices.size();
		}
	}

	void DrawDebug(RenderScene& scene, const CameraComponent& camera, GPU::CommandList& cmd)
	{
		cmd.BeginEvent("DrawDebug");

		// Draw lines
		if (!debugDrawData.renderableLines.empty())
		{
			cmd.BeginEvent("DebugLines");
			cmd.SetPipelineState(debugLinePSO);
			cmd.SetVertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
			cmd.SetVertexAttribute(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(F32x4));
			cmd.SetProgram(
				Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetVS("VS"),
				Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetPS("PS")
			);

			DebugConstant cb;
			cb.pos = StoreFMat4x4(camera.GetViewProjection());
			cb.col = F32x4(1.0f);
			cmd.BindConstant<DebugConstant>(cb, 0, 0);

			struct LineSegment
			{
				F32x4 pointA;
				F32x4 colorA;
				F32x4 pointB;
				F32x4 colorB;
			};
			LineSegment* vertMem = static_cast<LineSegment*>(cmd.AllocateVertexBuffer(0, 
				sizeof(LineSegment) * debugDrawData.renderableLines.size(),
				sizeof(F32x4) + sizeof(F32x4),
				VK_VERTEX_INPUT_RATE_VERTEX));

			I32 numSegments = 0;
			for (const auto& line : debugDrawData.renderableLines)
			{
				LineSegment segment;
				segment.pointA = F32x4(line.start, 1);
				segment.pointB = F32x4(line.end, 1);
				segment.colorA = line.color;
				segment.colorB = line.color;
		
				memcpy(vertMem + numSegments, &segment, sizeof(LineSegment));
				numSegments++;
			}

			cmd.Draw(numSegments * 2);
			debugDrawData.renderableLines.clear();
			cmd.EndEvent();
		}

		// Draw boxes
		if (!debugDrawData.renderableBoxes.empty())
		{
			SetupWireCubeResources();

			cmd.BeginEvent("DebugBoxes");
			cmd.SetPipelineState(debugPSO);
			cmd.SetVertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
			cmd.SetVertexAttribute(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(F32x4));
			cmd.SetProgram(
				Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetVS("VS"),
				Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetPS("PS")
			);

			cmd.BindVertexBuffer(wireCubeVB, 0, 0, sizeof(F32x4) + sizeof(F32x4), VK_VERTEX_INPUT_RATE_VERTEX);
			cmd.BindIndexBuffer(wireCubeIB, 0, VK_INDEX_TYPE_UINT16);

			DebugConstant cb;
			for (const auto& pair : debugDrawData.renderableBoxes)
			{
				cb.pos = StoreFMat4x4(LoadFMat4x4(pair.first) * camera.GetViewProjection());
				cb.col = pair.second;
				cmd.BindConstant<DebugConstant>(cb, 0, 0);
				cmd.DrawIndexed(24);
			}
			debugDrawData.renderableBoxes.clear();

			cmd.EndEvent();
		}

		// Draw spheres
		if (!debugDrawData.renderableSpheres.empty())
		{
			SetupWireSphereResources();

			cmd.BeginEvent("DebugSpheres");
			cmd.SetPipelineState(debugPSO);
			cmd.SetVertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
			cmd.SetVertexAttribute(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(F32x4));
			cmd.SetProgram(
				Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetVS("VS"),
				Renderer::GetShader(ShaderType::SHADERTYPE_VERTEXCOLOR)->GetPS("PS")
			);

			cmd.BindVertexBuffer(wireSphereVB, 0, 0, sizeof(F32x4) + sizeof(F32x4), VK_VERTEX_INPUT_RATE_VERTEX);
			cmd.BindIndexBuffer(wireSphereIB, 0, VK_INDEX_TYPE_UINT16);

			DebugConstant cb;
			for (const auto& pair : debugDrawData.renderableSpheres)
			{
				Sphere sphere = pair.first;
				MATRIX mat = MatrixScaling(sphere.radius, sphere.radius, sphere.radius) *
					MatrixTranslation(sphere.center.x, sphere.center.y, sphere.center.z) *
					camera.GetViewProjection();
				cb.pos = StoreFMat4x4(mat);
				cb.col = pair.second;
				cmd.BindConstant<DebugConstant>(cb, 0, 0);
				cmd.DrawIndexed(wireSphereIndices);
			}
			debugDrawData.renderableSpheres.clear();

			cmd.EndEvent();
		}

		cmd.EndEvent();
	}

	void DrawLine(const F32x3 p1, const F32x3 p2, const F32x4& color)
	{
		debugDrawData.renderableLines.push_back({p1, p2, color});
	}

	void DrawBox(const FMat4x4& boxMatrix, const F32x4& color)
	{
		debugDrawData.renderableBoxes.push_back(ToPair(boxMatrix, color));
	}

	void DrawSphere(const Sphere& sphere, const F32x4& color)
	{
		debugDrawData.renderableSpheres.push_back(ToPair(sphere, color));
	}
}
}