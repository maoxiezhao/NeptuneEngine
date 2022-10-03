#pragma once

#include "content\resource.h"
#include "definition.h"

namespace VulkanTest
{
namespace Editor
{
	class ResourceImportingManager
	{
	public:	
		// Resource Importers
		static Array<ResourceImporter> importers;
		static const ResourceImporter* GetImporter(const String& extension);

		// Resource creators
		static Array<ResourceCreator> creators;
		static const ResourceCreator* GetCreator(const String& tag);

		static const String CreateMaterialTag;
		static const String CreateModelTag;

	public:
		static bool Create(const String& tag, Guid& guid, const Path& targetPath, void* arg = nullptr, bool isCompiled = true);
		static bool Create(CreateResourceFunction createFunc, Guid& guid, const Path& inputPath, const Path& outputPath, void* arg = nullptr, bool isCompiled = true);
		static bool Create(CreateResourceFunction createFunc, const Path& inputPath, const Path& outputPath, void* arg = nullptr, bool isCompiled = true)
		{
			Guid guid = Guid::Empty;
			return Create(createFunc, guid, inputPath, outputPath, arg, isCompiled);
		}

		static bool Import(const Path& inputPath, const Path& outputPath, Guid& resID, void* arg);
	};
}
}
