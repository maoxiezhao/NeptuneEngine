#include "materialImporter.h"
#include "editor\importers\resourceImportingManager.h"
#include "editor\editor.h"
#include "editor\widgets\assetCompiler.h"

namespace VulkanTest
{
namespace Editor
{
	bool MaterialImporter::Import(EditorApp& app, const Path& path)
	{
		auto srcStorage = StorageManager::GetStorage(path, true, false);
		if (!srcStorage)
		{
			Logger::Error("failed to read file:%s", path.c_str());
			return false;
		}

		ResourceInitData initData;
		if (!srcStorage->LoadResourceHeader(initData))
			return false;

		auto srcDataChunk = initData.header.chunks[0];
		if (!srcStorage->LoadChunk(srcDataChunk))
			return false;

		// Write compiled resource
		const Path& taretPath = ResourceStorage::GetContentPath(path, true);
		return ResourceImportingManager::Create([&](CreateResourceContext& ctx)->CreateResult {
		
			MaterialHeader header;
			memcpy(&header.materialInfo, initData.customData.Data(), sizeof(MaterialInfo));
			ctx.WriteCustomData(header);

			// Shader chunk
			auto shaderData = ctx.AllocateChunk(MATERIAL_CHUNK_SHADER_SOURCE);
			if (shaderData)
			{
				// TODO Compile generated shader soruce code
			}

			// Param chunk
			auto paramsData = ctx.AllocateChunk(MATERIAL_CHUNK_PARAMS);
			if (paramsData)
			{
				paramsData->mem.Link(srcDataChunk->Data(), srcDataChunk->Size());
			}

			return CreateResult::Ok;
		}, initData.header.guid, path, taretPath);
	}
}
}
