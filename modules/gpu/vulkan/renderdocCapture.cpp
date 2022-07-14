#include "device.h"
#include "core\platform\platform.h"
#include "renderdoc\renderdoc_app.h"

namespace VulkanTest
{
namespace GPU
{

#define CiLoadLibrary(name) LoadLibrary(_T(name))

static RENDERDOC_API_1_4_1* renderdocAPI;

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

    auto* gpa = GetProcAddress(renderdocModule, "RENDERDOC_GetAPI");
    pRENDERDOC_GetAPI func;
    memcpy(&func, &gpa, sizeof(func));

    if (!func)
    {
        Logger::Error("Failed to load RENDERDOC_GetAPI function.");
        return false;
    }

    if (!func(eRENDERDOC_API_Version_1_4_0, reinterpret_cast<void**>(&renderdocAPI)))
    {
        Logger::Error("Failed to obtain RenderDoc 1.4.0 API.");
        return false;
    }
    else
    {
        int major, minor, patch;
        renderdocAPI->GetAPIVersion(&major, &minor, &patch);
        Logger::Info("Initialized RenderDoc API %d.%d.%d.", major, minor, patch);
    }

    return true;
}
}
}