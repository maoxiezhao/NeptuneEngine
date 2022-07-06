#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\collections\array.h"
#include "core\collections\hashMap.h"
#include "core\platform\platform.h"
#include "core\utils\string.h"
#include "core\utils\delegate.h"
#include "core\utils\profiler.h"
#include "core\scripts\luaUtils.h"
#include "core\scene\world.h"
#include "math\hash.h"

#ifdef STATIC_PLUGINS
#define VULKAN_EDITOR_API
#elif defined BUILDING_ENGINE
#define VULKAN_EDITOR_API LIBRARY_EXPORT
#else
#define VULKAN_EDITOR_API LIBRARY_IMPORT
#endif