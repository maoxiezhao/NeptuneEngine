#pragma once

#include "core\resource\binaryResource.h"
#include "core\serialization\stream.h"
#include "core\collections\array.h"
#include "core\scene\world.h"
#include "core\streaming\streaming.h"
#include "renderer\materials\material.h"
#include "gpu\vulkan\device.h"
#include "math\geometry.h"

namespace VulkanTest
{
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

		Mesh(const GPU::InputLayout& inputLayout_,
			U8 stride_,
			const char* name_,
			const AttributeSemantic* semantics_);

		GPU::InputLayout inputLayout;
		String name;
		U8 stride;
		Mesh::AttributeSemantic semantics[GPU::InputLayout::MAX_ATTRIBUTES];
		AABB aabb;

		Array<F32x3> vertexPos;
		Array<F32x3> vertexNor;
		Array<F32x4> vertexTangents;
		Array<F32x2> vertexUV;
		Array<U32> indices;

		struct MeshSubset
		{
			ResPtr<Material> material;
			U32 indexOffset = 0;
			U32 indexCount = 0;

			ECS::Entity materialID = ECS::INVALID_ENTITY;
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

		GPU::BufferPtr generalBuffer;
		
		struct BufferView
		{
			U64 offset = ~0ull;
			U64 size = 0ull;
			GPU::BindlessDescriptorPtr srv;

			constexpr bool IsValid() const {
				return offset != ~0ull;
			}
		};
		BufferView ib;
		BufferView vbPos;
		BufferView vbNor;
		BufferView vbTan;
		BufferView vbUVs;

		bool CreateRenderData();

		PickResult CastRayPick(const VECTOR& rayOrigin, const VECTOR& rayDirection, F32 tmin, F32 tmax);

	};

	class VULKAN_TEST_API Model final : public BinaryResource
	{
	public:
		DECLARE_RESOURCE(Model);

#pragma pack(1)
		struct FileHeader
		{
			U32 magic;
			U32 version;
		};
#pragma pack()
		static const U32 FILE_MAGIC;
		static const U32 FILE_VERSION;

		Model(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Model();
	
		Mesh& GetMesh(U32 index) { 
			return meshes[index];
		}
		const Mesh& GetMesh(U32 index) const { 
			return meshes[index]; 
		}
		I32 GetMeshCount() const { 
			return meshes.size();
		}

	protected:
		bool Init(ResourceInitData& initData)override;
		bool Load()override;
		void Unload() override;

	private:
		Model(const Model&) = delete;
		void operator=(const Model&) = delete;

		bool ParseMeshes(InputMemoryStream& mem);

		FileHeader header;
		Array<Mesh> meshes;
	};
}