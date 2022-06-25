#pragma once

#include "core\resource\resource.h"
#include "core\utils\stream.h"
#include "core\collections\array.h"
#include "renderer\material.h"
#include "gpu\vulkan\device.h"

namespace VulkanTest
{
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

		Mesh(ResPtr<Material>&& mat_,
			const GPU::InputLayout& inputLayout_,
			U8 stride_,
			const char* name_,
			const AttributeSemantic* semantics_);

		ResPtr<Material> material;
		GPU::InputLayout inputLayout;
		String name;
		U8 stride;
		Mesh::AttributeSemantic semantics[GPU::InputLayout::MAX_ATTRIBUTES];

		Array<F32x3> vertexPos;
		Array<F32x3> vertexNor;
		Array<F32x2> vertexUV;
		Array<U32> vertexColor;
		Array<U32> indices;

		GPU::BufferPtr vboPos;
		GPU::BufferPtr vboNor;
		GPU::BufferPtr vboUV;
		GPU::BufferPtr ibo;

		void CreateRenderData();
	};

	class VULKAN_TEST_API Model final : public Resource
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
	
	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;

	private:
		Model(const Model&) = delete;
		void operator=(const Model&) = delete;

		bool ParseMeshes(InputMemoryStream& mem);

		Array<Mesh> meshes;
	};
}