#pragma once

#include "renderer\rendererCommon.h"

namespace VulkanTest
{
	class Model;

	struct VULKAN_TEST_API PickResult
	{
		ECS::Entity entity = ECS::INVALID_ENTITY;
		F32x3 position = F32x3(0.0f);
		F32x3 normal = F32x3(0.0f);
		F32 distance = std::numeric_limits<float>::max();
		bool isHit = false;
	};

	class VULKAN_TEST_API Mesh
	{
	public:
		enum class AttributeSemantic : U8 
		{
			POSITION,
			NORMAL,
			TANGENT,
			COLOR0,
			COLOR1,
			INDICES,
			TEXCOORD0,
			TEXCOORD1,

			INSTANCE0,
			INSTANCE1,
			INSTANCE2,

			COUNT,
			NONE = 0xff
		};

		enum class AttributeType : U8 
		{
			F32,
			I32,
			COUNT
		};

		void Init(const char* name_, Model* model_, I32 lodIndex_, I32 index_, const AABB& aabb_);
		bool Load();
		void Unload();
		bool IsReady()const;

		inline GPU::IndexBufferFormat GetIndexFormat() const { 
			return vertexPos.size() > 65536 ? GPU::IndexBufferFormat::UINT32 : GPU::IndexBufferFormat::UINT16; 
		}

		inline size_t GetIndexStride() const { 
			return GetIndexFormat() == GPU::IndexBufferFormat::UINT32 ? sizeof(U32) : sizeof(U16);
		}

	public:
		String name;
		Model* model = nullptr;
		I32 index = 0;
		I32 lodIndex = 0;
		AABB aabb;

		Array<F32x3> vertexPos;
		Array<F32x3> vertexNor;
		Array<F32x4> vertexTangents;
		Array<F32x2> vertexUV;
		Array<U32> indices;

		struct MeshSubset
		{
			I32 materialIndex = -1;
			U32 indexOffset = 0;
			U32 indexCount = 0;
			ECS::Entity material = ECS::INVALID_ENTITY;
		};
		Array<MeshSubset> subsets;

		struct LODMeshIndices
		{
			int from;
			int to;
		};
		Array<LODMeshIndices> lodIndices;
		LODMeshIndices GetLODMeshIndex(U32 lod)
		{
			if (lodIndices.empty())
				return { 0, (int)subsets.size() };

			return lodIndices[lod];
		}

	public:
		GPU::BufferPtr generalBuffer;

		struct BufferView
		{
			U64 offset = ~0ull;
			U64 size = 0ull;
			GPU::BindlessDescriptorPtr srv;

			constexpr bool IsValid() const {
				return offset != ~0ull;
			}

			void Reset()
			{
				offset = ~0ull;
				size = 0ull;
				srv.reset();
			}
		};
		GPU::BufferViewPtr ibView;
		BufferView ib;
		BufferView vbPos;
		BufferView vbNor;
		BufferView vbTan;
		BufferView vbUVs;

		PickResult CastRayPick(const VECTOR& rayOrigin, const VECTOR& rayDirection, F32 tmin, F32 tmax);
	};
}