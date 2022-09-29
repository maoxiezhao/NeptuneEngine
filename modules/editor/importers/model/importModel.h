#pragma once

#include "modelImporter.h"

namespace VulkanTest
{
namespace Editor
{
	bool ImportModelDataObj(const char* path, ModelImporter::ImportModel& modelData, const ModelImporter::ImportConfig& cfg);
}
}
