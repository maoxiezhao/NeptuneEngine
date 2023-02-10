#pragma once

#include "Engine/Core/Config.h"

#if PLATFORM_WINDOWS
class WindowsPlatform;
typedef WindowsPlatform Platform;
#else
#error Missing Types implementation!
#endif