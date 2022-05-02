#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\collections\Array.h"
#include "core\utils\string.h"
#include "core\utils\delegate.h"

#ifdef STATIC_PLUGINS
#define VULKAN_EDITOR_API
#elif defined BUILDING_ENGINE
#define VULKAN_EDITOR_API LIBRARY_EXPORT
#else
#define VULKAN_EDITOR_API LIBRARY_IMPORT
#endif