#include "shaderCompiler.h"
#include "helper.h"

#ifdef CJING3D_PLATFORM_WIN32
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>
#include <atlbase.h> // ComPtr

#define CiLoadLibrary(name) LoadLibrary(_T(name))
#define CiGetProcAddress(handle,name) GetProcAddress(handle, name)

#define VK_USE_PLATFORM_WIN32_KHR
#endif

namespace ShaderCompiler
{
    CComPtr<IDxcCompiler3> dxcCompiler = nullptr;

	void Initialize()
	{
        HMODULE dxcompiler = CiLoadLibrary("dxcompiler.dll");
        if (dxcompiler != nullptr)
        {
            DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)CiGetProcAddress(dxcompiler, "DxcCreateInstance");
            if (DxcCreateInstance != nullptr)
            {
                HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
                assert(SUCCEEDED(hr));
            }
        }
	}

	bool LoadShader(ShaderStage stage, Shader& shader, const std::string& filePath)
	{
		return false;
	}
}

