#include "device.h"
#include "core\platform\platform.h"

namespace VulkanTest
{
namespace GPU
{

#define CiLoadLibrary(name) LoadLibrary(_T(name))

static std::mutex mutex;
#ifdef _WIN32
    static HMODULE renderdocModule;
#else
    static void* renderdocModule;
#endif

bool DeviceVulkan::InitRenderdocCapture()
{
#ifdef _WIN32
    renderdocModule = CiLoadLibrary("renderdoc.dll");
#else
    renderdocModule = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
#endif
    if (!renderdocModule)
    {
        Logger::Error("Failed to load RenderDoc, make sure RenderDoc started the application in capture mode.\n");
        return false;
    }

    return true;
}
}
}