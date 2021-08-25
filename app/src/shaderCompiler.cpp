#include "shaderCompiler.h"
#include "helper.h"
#include "archive.h"

#define SHADERCOMPILER_ENABLED

#ifdef CJING3D_PLATFORM_WIN32
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>
#include <atlbase.h> // ComPtr

#define CiLoadLibrary(name) LoadLibrary(_T(name))
#define CiGetProcAddress(handle,name) GetProcAddress(handle, name)

#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <unordered_set>
#include <filesystem>

namespace ShaderCompiler
{
    namespace
    {
		const std::string EXPORT_SHADER_PATH = ".export/shaders";
		const std::string SOURCE_SHADER_PATH = "shaders/";

		struct CompilerInput
		{
			ShaderStage stage = ShaderStage::Count;
			std::string shadersourcefilename;
			std::string entrypoint = "main";
			std::vector<std::string> includeDirectories;
			std::vector<std::string> defines;
		};
		struct CompilerOutput
		{
			const uint8_t* shaderdata = nullptr;
			size_t shadersize = 0;
			std::vector<uint8_t> shaderhash;
			std::string errorMessage;
			std::vector<std::string> dependencies;
		};

		CComPtr<IDxcCompiler3> dxcCompiler = nullptr;
		std::unordered_set<std::string> registeredShaders;

		void RegisterShader(const std::string& shaderfilename)
		{
#ifdef SHADERCOMPILER_ENABLED
			registeredShaders.insert(shaderfilename);
#endif
		}

		bool IsShaderOutdated(const std::string& shaderfilename)
		{
#ifdef SHADERCOMPILER_ENABLED
			std::string filepath = shaderfilename;
			Helper::MakePathAbsolute(filepath);
			if (!Helper::FileExists(filepath))
				return true; // no shader file = outdated shader, apps can attempt to rebuild it

			std::string dependencylibrarypath = Helper::ReplaceExtension(shaderfilename, "shadermeta");
			if (!Helper::FileExists(dependencylibrarypath))
				return false; // no metadata file = no dependency, up to date (for example packaged builds)

			const auto tim = std::filesystem::last_write_time(filepath);

			Archive dependencyLibrary(dependencylibrarypath);
			if (dependencyLibrary.IsOpen())
			{
				std::string rootdir = dependencyLibrary.GetSourceDirectory();
				std::vector<std::string> dependencies;
				dependencyLibrary >> dependencies;

				for (auto& x : dependencies)
				{
					std::string dependencypath = rootdir + x;
					Helper::MakePathAbsolute(dependencypath);
					if (Helper::FileExists(dependencypath))
					{
						const auto dep_tim = std::filesystem::last_write_time(dependencypath);

						if (tim < dep_tim)
						{
							return true;
						}
					}
				}
			}
#endif // SHADERCOMPILER_ENABLED

			return false;
		}

		bool SaveShaderAndMetadata(const std::string& shaderfilename, const CompilerOutput& output)
		{
#ifdef SHADERCOMPILER_ENABLED
			Helper::DirectoryCreate(Helper::GetDirectoryFromPath(shaderfilename));

			Archive dependencyLibrary(Helper::ReplaceExtension(shaderfilename, "shadermeta"), false);
			if (dependencyLibrary.IsOpen())
			{
				std::string rootdir = dependencyLibrary.GetSourceDirectory();
				std::vector<std::string> dependencies = output.dependencies;
				for (auto& x : dependencies)
				{
					Helper::MakePathRelative(rootdir, x);
				}
				dependencyLibrary << dependencies;
			}

			if (Helper::FileWrite(shaderfilename, output.shaderdata, output.shadersize))
			{
				return true;
			}
#endif // SHADERCOMPILER_ENABLED

			return false;
		}

		bool Compile(const CompilerInput& input, CompilerOutput& output)
		{
			return false;
		}
	}
    
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

	bool LoadShader(DeviceVulkan& device, ShaderStage stage, Shader& shader, const std::string& filePath)
	{
		std::string exportShaderPath = EXPORT_SHADER_PATH + filePath;
		RegisterShader(exportShaderPath);

		if (IsShaderOutdated(exportShaderPath))
		{
			std::string sourcedir = SOURCE_SHADER_PATH;
			Helper::MakePathAbsolute(sourcedir);

			CompilerInput input;
			input.stage = stage;
			input.includeDirectories.push_back(sourcedir);
			input.shadersourcefilename = Helper::ReplaceExtension(sourcedir + filePath, "hlsl");

			CompilerOutput output;
			if (Compile(input, output))
			{
				return device.CreateShader(stage, output.shaderdata, output.shadersize, &shader);
			}
			else
			{
				std::cout << "Failed to compile shader:" + filePath << std::endl;
			}
		}

		std::vector<uint8_t> buffer;
		if (Helper::FileRead(exportShaderPath, buffer))
			return device.CreateShader(stage, buffer.data(), buffer.size(), &shader);

		return false;
	}
}

