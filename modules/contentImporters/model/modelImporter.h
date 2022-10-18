#pragma once

#include "content\resources\texture.h"
#include "content\resources\material.h"
#include "contentImporters\definition.h"
#include "math\geometry.h"
#include "level\scene.h"

namespace VulkanTest
{
namespace Editor
{
	class VULKAN_TEST_API ModelImporter
	{
	public:
		enum class ModelType
		{
			Model,
			Animation,
			Scene
		};

		struct ImportConfig
		{
			ModelType type = ModelType::Model;
			F32 scale = 1.0f;
			bool autoLODs = false;
			U32 autoLodCount = 1;
		};

		struct ImportMesh
		{
			std::string name;
			U32 lod = 0;
			AABB aabb;
			bool import = true;

			struct MeshSubset
			{
				I32 materialIndex = -1;
				U32 indexOffset = 0;
				U32 indexCount = 0;
				U32 uniqueIndexOffset = 0;
				U32 uniqueIndexCount = 0;
			};
			Array<MeshSubset> subsets;

			Array<F32x3> vertexPositions;
			Array<F32x3> vertexNormals;
			Array<F32x4> vertexTangents;
			Array<F32x2> vertexUvset_0;
			Array<U32> indices;
			bool hasUV = false;
		};

		struct ImportTexture
		{
			Guid guid = Guid::Empty;
			Texture::TextureType type;
			bool import = true;
			bool toDDS = true;
			bool isValid = false;
			StaticString<MAX_PATH_LENGTH> path;
		};

		struct ImportMaterial
		{
			Guid guid = Guid::Empty;
			String name;
			ImportTexture textures[Texture::TextureType::COUNT];
			bool import = true;

			F32x4 color = F32x4(1.0f);
			F32 metallic = 0.0f;
			F32 roughness = 1.0f;
			bool doubleSided = false;
			BlendMode blendMode = BlendMode::BLENDMODE_OPAQUE;
			F32 alphaRef = 1.0f;
		};

		struct ImportLOD
		{
			Array<ImportMesh*> meshes;
			I32 lodIndex = 0;
		};

		struct ImportNode
		{
			String name;
			Transform transform;
			I32 meshIndex = -1;
			Array<ImportNode> children;
		};

		struct ImportModel
		{
			Array<ImportMesh> meshes;
			Array<ImportLOD> lods;
			Array<ImportTexture> textures;
			Array<ImportMaterial> materials;
			ImportNode root;
		};

	public:
		static CreateResult Import(CreateResourceContext& ctx);
		static CreateResult WriteModel(CreateResourceContext& ctx, ImportModel& modelData);
		static CreateResult WriteScene(CreateResourceContext& ctx, ImportModel& modelData, const Path& importOutput);
		static CreateResult Create(CreateResourceContext& ctx);

	private:
		static bool WriteMesh(OutputMemoryStream& outMem, const ImportMesh& mesh);

		struct WriteSceneContext
		{
			ImportModel& modelData;
			CreateResourceContext& ctx;
			Scene* scene;

			WriteSceneContext(CreateResourceContext& ctx_, ImportModel& modelData_, Scene* scene_) :
				ctx(ctx_),
				modelData(modelData_),
				scene(scene_)
			{}
		};
		static bool WriteSceneNode(WriteSceneContext& ctx, const ImportNode& node, ECS::Entity parent, const Path& importOutput);
	};
}
}
