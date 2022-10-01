#pragma once

#include "modelImporter.h"

namespace VulkanTest
{
namespace Editor
{
	bool ImportModelDataOBJ(const char* path, ModelImporter::ImportModel& modelData, const ModelImporter::ImportConfig& cfg);
	bool ImportModelDataGLTF(const char* path, ModelImporter::ImportModel& modelData, const ModelImporter::ImportConfig& cfg);
}
}
