#pragma once

#include "editorPlugin.h"
#include "renderer\texture.h"
#include "loader\tiny_obj_loader.h"
#include "math\geometry.h"

namespace VulkanTest
{
namespace Editor
{
	class VULKAN_EDITOR_API OBJImporter
	{
	public:
		OBJImporter(class EditorApp& editor_);
		~OBJImporter();

		struct ImportConfig
		{
			F32 scale;
			U32 autoLodCount = 1;
			bool autoLODs = false;
		};

		struct ImportMesh
		{
			const tinyobj::mesh_t* mesh = nullptr;
			
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
			Texture::TextureType type;
			bool import = true;
			bool toDDS = true;
			bool isValid = false;
			StaticString<MAX_PATH_LENGTH> path;
		};

		struct ImportMaterial
		{
			Guid guid;
			const tinyobj::material_t* material = nullptr;		
			ImportTexture textures[Texture::TextureType::COUNT];
			bool import = true;
		};

		struct ImportLOD
		{
			Array<ImportMesh*> meshes;
			I32 lodIndex = 0;
		};

		bool ImportModel(const char* filename);
		bool WriteModel(const char* filepath, const ImportConfig& cfg);

	private:
		void PostprocessMeshes(const ImportConfig& cfg);
		void GetImportMeshName(const ImportMesh& mesh, char(&out)[256]);
		void WriteMesh(OutputMemoryStream& outmem, const ImportMesh& mesh);
		bool AreIndices16Bit(const ImportMesh& mesh) const;
		I32  GetAttributeCount(const ImportMesh& mesh)const;
		bool WriteMaterials(const char* filepath, const ImportConfig& cfg);

	private:
		EditorApp& editor;

		tinyobj::attrib_t objAttrib;
		std::vector<tinyobj::shape_t> objShapes;
		std::vector<tinyobj::material_t> objMaterials;

		Array<ImportMesh> meshes;
		Array<ImportLOD> lods;
		Array<ImportMaterial> materials;
	};
}
}
