#include "materialImporter.h"
#include "editor\editor.h"
#include "editor\widgets\assetCompiler.h"

namespace VulkanTest
{
namespace Editor
{
	bool MaterialImporter::Import(EditorApp& app, const Path& path)
	{
		FileSystem& fs = app.GetEngine().GetFileSystem();
		OutputMemoryStream mem;
		if (!fs.LoadContext(path.c_str(), mem))
		{
			Logger::Error("failed to read file:%s", path.c_str());
			return false;
		}

		ResourceDataWriter resWriter(Material::ResType);
		InputMemoryStream input(mem);

		MaterialInfo materialInfo;
		input.Read<MaterialInfo>(materialInfo);
		MaterialHeader header = {};
		header.materialInfo = materialInfo;
		resWriter.WriteCustomData(header);

		// Shader chunk
		auto shaderData = resWriter.GetChunk(MATERIAL_CHUNK_SHADER_SOURCE);
		if (shaderData)
		{
			if (materialInfo.type == MaterialType::Visual)
			{
				// Compile generated shader soruce code
				// TODO
			}
		}

		// Param chunk
		auto paramsData = resWriter.GetChunk(MATERIAL_CHUNK_PARAMS);
		if (paramsData)
		{
			paramsData->mem.Write((const U8*)input.GetBuffer() + input.GetPos(), input.Size() - sizeof(MaterialInfo));
		}

		return app.GetAssetCompiler().WriteCompiled(path.c_str(), resWriter.data);
	}
}
}
