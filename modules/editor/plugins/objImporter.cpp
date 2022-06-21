#include "objImporter.h"
#include "editor\editor.h"
#include "core\filesystem\filesystem.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "loader\tiny_obj_loader.h"

namespace VulkanTest
{
namespace Editor
{
	struct membuf : std::streambuf
	{
		membuf(char* begin, char* end) 
		{
			this->setg(begin, begin, end);
		}
	};

	// Custom material file reader:
	class MaterialFileReader : public tinyobj::MaterialReader 
	{
	public:
		explicit MaterialFileReader(FileSystem& fs_, const std::string& basedir_)
			: basedir(basedir_), fs(fs_) {}

		virtual bool operator()(
			const std::string& matId,
			std::vector<tinyobj::material_t>* materials,
			std::map<std::string, int>* matMap, 
			std::string* err)
		{
			std::string filepath;
			if (!basedir.empty()) {
				filepath = std::string(basedir) + matId;
			}
			else {
				filepath = matId;
			}

			OutputMemoryStream mem;
			if (!fs.LoadContext(filepath.c_str(), mem))
			{
				std::string ss;
				ss += "WARN: Material file [ " + filepath + " ] not found.\n";
				if (err) {
					(*err) += ss;
				}
				return false;
			}
			membuf sbuf((char*)mem.Data(), (char*)mem.Data() + mem.Size());
			std::istream matIStream(&sbuf);

			std::string warning;
			LoadMtl(matMap, materials, &matIStream, &warning);

			if (!warning.empty()) {
				if (err) {
					(*err) += warning;
				}
			}

			return true;
		}

	private:
		FileSystem& fs;
		std::string basedir;
	};

	OBJImporter::OBJImporter(EditorApp& editor_) :
		editor(editor_)
	{
	}

	OBJImporter::~OBJImporter()
	{
	}

	bool OBJImporter::Import(const char* filename)
	{
		PROFILE_FUNCTION();

		objShapes.clear();
		objMaterials.clear();

		FileSystem& fs = editor.GetEngine().GetFileSystem();
		OutputMemoryStream mem;
		if (!fs.LoadContext(filename, mem))
			return false;

		char srcDir[MAX_PATH_LENGTH];
		CopyString(Span(srcDir), Path::GetDir(filename));

		std::string objErrors;
		membuf sbuf((char*)mem.Data(), (char*)mem.Data() + mem.Size());
		std::istream in(&sbuf);
		MaterialFileReader matFileReader(fs, srcDir);
		if (!tinyobj::LoadObj(&objAttrib, &objShapes, &objMaterials, &objErrors, &in, &matFileReader, true))
		{
			if (!objErrors.empty())
				Logger::Error(objErrors.c_str());
			return false;
		}
	
		for (auto& shape : objShapes)
		{
			// Gather geometries
			ImportGeometry& geom = geometries.emplace();
			geom.shape = &shape;

			// Gather meshes
			const size_t matCount = shape.mesh.material_ids.size();
			for (I32 j = 0; j < matCount; j++)
			{
				auto& mesh = meshes.emplace();
				mesh.mesh = &shape.mesh;
				mesh.material = &objMaterials[shape.mesh.material_ids[j]];
			}
		}

		// Gather materials
		for (auto& mesh : meshes)
		{
			if (mesh.material == nullptr)
				continue;

			ImportMaterial& mat = materials.emplace();
			mat.material = mesh.material;

			auto GatherTexture = [this, &mat, &fs, srcDir](Texture::TextureType type) {
				
				std::string texname;
				switch (type)
				{
				case Texture::DIFFUSE:
					texname = mat.material->diffuse_texname;
					break;
				case Texture::NORMAL:
					texname = mat.material->normal_texname;
					break;
				case Texture::SPECULAR:
					texname = mat.material->specular_texname;
					break;
				default:
					break;
				}
				if (texname.empty())
					return;

				ImportTexture& tex = mat.textures[(U32)type];
				tex.type = type;
				tex.path = Path::Join(Span(srcDir), Span(texname.c_str(), texname.size()));
				tex.isValid = fs.FileExists(tex.path.c_str());
				tex.import = true;
			};

			GatherTexture(Texture::TextureType::DIFFUSE);
			GatherTexture(Texture::TextureType::NORMAL);
			GatherTexture(Texture::TextureType::SPECULAR);
		}

		return true;
	}

	void OBJImporter::Write(const char* filepath, const ImportConfig& cfg)
	{
	}
}
}