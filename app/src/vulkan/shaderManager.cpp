#include "shaderManager.h"
#include "shaderCompiler.h"
#include "device.h"
#include "utils\archive.h"
#include "utils\helper.h"

#include <unordered_set>
#include <filesystem>

using namespace ShaderCompiler;

#define RUNTIME_SHADERCOMPILER_ENABLED

namespace {

	const std::string EXPORT_SHADER_PATH = ".export/shaders/";
	const std::string SOURCE_SHADER_PATH = "shaders/";

	std::unordered_set<std::string> registeredShaders;

	HashValue GetPathHash(ShaderStage stage, const std::string& path)
	{
		HashCombiner hasher;
		hasher.HashCombine(static_cast<U32>(stage));
		hasher.HashCombine(path.c_str());
		return hasher.Get();
	}

	void RegisterShader(const std::string& shaderfilename)
	{
#ifdef RUNTIME_SHADERCOMPILER_ENABLED
		registeredShaders.insert(shaderfilename);
#endif
	}

	bool IsShaderOutdated(const std::string& shaderfilename)
	{
#ifdef RUNTIME_SHADERCOMPILER_ENABLED
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
}

ShaderTemplate::ShaderTemplate(DeviceVulkan& device, const std::string& path, HashValue pathHash) :
	mDevice(device),
	mPath(path),
	mPathHash(pathHash)
{
}

ShaderTemplate::~ShaderTemplate()
{
}

bool ShaderTemplate::Initialize()
{
	if (!ShaderCompiler::Preprocess())
		return false;
	
	return true;
}

ShaderTemplateProgram::ShaderTemplateProgram(DeviceVulkan& device, ShaderStage stage, ShaderTemplate* shaderTemplate) :
	mDevice(device)
{
	SetShader(stage, shaderTemplate);
}

ShaderTemplateProgramVariant* ShaderTemplateProgram::RegisterVariant(const ShaderVariantMap& defines)
{
	//std::string exportShaderPath = EXPORT_SHADER_PATH + filePath;
	//RegisterShader(exportShaderPath);

	//if (IsShaderOutdated(exportShaderPath))
	//{
	//	std::string sourcedir = SOURCE_SHADER_PATH;
	//	Helper::MakePathAbsolute(sourcedir);

	//	CompilerInput input;
	//	input.stage = stage;
	//	input.includeDirectories.push_back(sourcedir);
	//	input.shadersourcefilename = Helper::ReplaceExtension(sourcedir + filePath, "hlsl");

	//	CompilerOutput output;
	//	if (Compile(input, output))
	//	{
	//		std::cout << "Compile shader successfully:" + filePath << std::endl;
	//		SaveShaderAndMetadata(exportShaderPath, output);
	//		if (!output.errorMessage.empty())
	//			Logger::Error(output.errorMessage.c_str());


	//		return false;
	//		//return device.CreateShader(stage, output.shaderdata, output.shadersize, &shader);
	//	}
	//	else
	//	{
	//		std::cout << "Failed to compile shader:" + filePath << std::endl;
	//		if (!output.errorMessage.empty())
	//			Logger::Error(output.errorMessage.c_str());
	//	}
	//}

	return nullptr;
}

void ShaderTemplateProgram::SetShader(ShaderStage stage, ShaderTemplate* shader)
{
	mShaderTemplates[static_cast<U32>(stage)] = shader;
}

Shader* ShaderTemplateProgramVariant::GetShader(ShaderStage stage)
{
	return nullptr;
}


ShaderManager::ShaderManager(DeviceVulkan& device) :
	mDevice(device)
{
	// init dxcompiler
	ShaderCompiler::Initialize();
}

ShaderManager::~ShaderManager()
{
}

Shader* ShaderManager::LoadShader(ShaderStage stage, const std::string& filePath, const ShaderVariantMap& defines)
{
	ShaderTemplateProgram* programTemplate = RegisterShader(stage, filePath);
	if (programTemplate == nullptr)
		return nullptr;

	ShaderTemplateProgramVariant* variant = programTemplate->RegisterVariant(defines);
	if (variant == nullptr)
		return nullptr;

	return variant->GetShader(stage);
}

ShaderTemplateProgram* ShaderManager::RegisterShader(ShaderStage stage, const std::string& filePath)
{
	ShaderTemplate* templ = GetTemplate(stage, filePath);
	if (templ == nullptr)
		return nullptr;

	HashCombiner hasher;
	hasher.HashCombine(templ->GetPathHash());
	auto it = mPrograms.find(hasher.Get());
	if (it != mPrograms.end())
		return it->second;

	return mPrograms.emplace(hasher.Get(), mDevice, stage, templ);
}

ShaderTemplate* ShaderManager::GetTemplate(ShaderStage stage, const std::string filePath)
{
	HashValue hash = GetPathHash(stage, filePath);
	auto it = mShaders.find(hash);
	if (it != mShaders.end())
	{
		return it->second;
	}

	ShaderTemplate* ret = mShaders.allocate(mDevice, filePath, hash);
	if (ret == nullptr || !ret->Initialize())
	{
		mShaders.free(ret);
		return nullptr;
	}

	ret->SetHash(hash);
	mShaders.insert(hash, ret);
	return ret;
}