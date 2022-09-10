#pragma once

#include "content\resource.h"
#include "resourceCreator.h"

namespace VulkanTest
{
namespace Editor
{
	class ResourceImportingManager
	{
	public:	
		static Array<ResourceCreator> creators;
		static const ResourceCreator* GetCreator(const String& tag);

		static const String CreateMaterialTag;

	public:
		static bool Create(EditorApp& editor, const String& tag, Guid& guid, const Path& path, void* arg = nullptr, bool isCompiled = true);
		static bool Create(EditorApp& editor, CreateResourceFunction createFunc, Guid& guid, const Path& path, void* arg = nullptr, bool isCompiled = true);
		static bool Create(EditorApp& editor, CreateResourceFunction createFunc, const Path& path, void* arg = nullptr, bool isCompiled = true)
		{
			Guid guid = Guid::Empty;
			return Create(editor, createFunc, guid, path, arg, isCompiled);
		}

	};
}
}
