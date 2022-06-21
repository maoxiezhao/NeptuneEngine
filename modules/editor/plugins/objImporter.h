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

		struct ImportGeometry
		{
			const tinyobj::shape_t* shape = nullptr;
		};

		struct ImportMesh
		{
			const tinyobj::mesh_t* mesh = nullptr;
			const tinyobj::material_t* material = nullptr;
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
		void Write(const char* filepath, const ImportConfig& cfg);

	private:
		EditorApp& editor;

		tinyobj::attrib_t objAttrib;
		std::vector<tinyobj::shape_t> objShapes;
		std::vector<tinyobj::material_t> objMaterials;

		Array<ImportGeometry> geometries;
		Array<ImportMesh> meshes;
		Array<ImportMaterial> materials;
	};
}
}
