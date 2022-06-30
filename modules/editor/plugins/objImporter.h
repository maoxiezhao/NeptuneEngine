#pragma once

#include "editorPlugin.h"
#include "renderer\texture.h"
#include "loader\tiny_obj_loader.h"

namespace VulkanTest
{
namespace Editor
{
	class OBJImporter
	{
	public:
		OBJImporter(class EditorApp& editor_);
		~OBJImporter();

		struct ImportConfig
		{
			F32 scale;
		};

		struct ImportMesh
		{
			const tinyobj::mesh_t* mesh = nullptr;
			
			std::string name;
			U32 lod = 0;
			bool import = true;

			struct MeshSubset
			{
				const tinyobj::material_t* material = nullptr;
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
			const tinyobj::material_t* material = nullptr;		
			ImportTexture textures[Texture::TextureType::COUNT];
			bool import = true;
		};

		bool Import(const char* filename);
		void WriteModel(const char* filepath, const ImportConfig& cfg);
		void WriteMaterials(const char* filepath, const ImportConfig& cfg);

	private:
		void PostprocessMeshes(const ImportConfig& cfg);
		void GetImportMeshName(const ImportMesh& mesh, char(&out)[256]);
		void WriteHeader();
		void WriteMesh(const char* src, const ImportMesh& mesh);
		void WriteMeshes(const char* src, I32 meshIdx, const ImportConfig& cfg);
		void WriteGeometry(const ImportConfig& cfg);
		bool AreIndices16Bit(const ImportMesh& mesh) const;
		I32 GetAttributeCount(const ImportMesh& mesh)const;

		template <typename T> 
		void Write(const T& obj) {
			outMem.Write(&obj, sizeof(obj)); 
		}
		void Write(const void* ptr, size_t size) { 
			outMem.Write(ptr, size);
		}
		void WriteString(const char* str);

		EditorApp& editor;

		tinyobj::attrib_t objAttrib;
		std::vector<tinyobj::shape_t> objShapes;
		std::vector<tinyobj::material_t> objMaterials;

		Array<ImportMesh> meshes;
		Array<ImportMaterial> materials;
		OutputMemoryStream outMem;
	};
}
}
