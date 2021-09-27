#include "shaderManager.h"
#include "shaderCompiler.h"
#include "device.h"
#include "utils\archive.h"
#include "utils\helper.h"

#include <unordered_set>
#include <filesystem>

using namespace ShaderCompiler;

#define RUNTIME_SHADERCOMPILER_ENABLED

// Shader flow:
// ShaderManager -> ShaderTemplateProgram -> ShaderTemplateProgramVariant 
// -> ShaderTemplate -> Variant -> Shader

namespace {

	const std::string EXPORT_SHADER_PATH = ".export/shaders/";
	const std::string SOURCE_SHADER_PATH = "shaders/";

	std::unordered_set<std::string> registeredShaders;
	std::unordered_map<HashValue, HashValue> shaderCache;

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
#endif // RUNTIME_SHADERCOMPILER_ENABLED

		return false;
	}

	bool SaveShaderAndMetadata(const std::string& shaderfilename, const CompilerOutput& output)
	{
#ifdef RUNTIME_SHADERCOMPILER_ENABLED
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
#endif // RUNTIME_SHADERCOMPILER_ENABLED

		return false;
	}
}

ShaderTemplate::ShaderTemplate(DeviceVulkan& device_, ShaderStage stage_, const std::string& path_, HashValue pathHash_) :
	device(device_),
	stage(stage_),
	path(path_),
	pathHash(pathHash_)
{
}

ShaderTemplate::~ShaderTemplate()
{
}

bool ShaderTemplate::Initialize()
{
#ifdef RUNTIME_SHADERCOMPILER_ENABLED
	if (!ShaderCompiler::Preprocess())
		return false;
#endif

	return true;
}

ShaderTemplate::Variant* ShaderTemplate::RegisterVariant(const ShaderVariantMap& defines)
{
	HashCombiner hasher;
	for (auto& define : defines)
		hasher.HashCombine(define.c_str());

	HashValue hash = hasher.Get();
	auto it = variants.find(hash);
	if (it != variants.end())
	{
		return it->second;
	}

	Variant* ret = variants.allocate();
	if (ret == nullptr)
	{
		variants.free(ret);
		return nullptr;
	}
	ret->hash = hash;

#ifdef RUNTIME_SHADERCOMPILER_ENABLED
	std::string exportShaderPath = EXPORT_SHADER_PATH + path;
	RegisterShader(exportShaderPath);

	if (IsShaderOutdated(exportShaderPath))
	{
		std::string sourcedir = SOURCE_SHADER_PATH;
		Helper::MakePathAbsolute(sourcedir);

		CompilerInput input;
		input.stage = stage;
		input.includeDirectories.push_back(sourcedir);
		input.defines = defines;
		input.shadersourcefilename = Helper::ReplaceExtension(sourcedir + path, "hlsl");

		CompilerOutput output;
		if (Compile(input, output))
		{
			Logger::Error("Compile shader successfully:%d", path.c_str());
			SaveShaderAndMetadata(exportShaderPath, output);
			if (!output.errorMessage.empty())
				Logger::Error(output.errorMessage.c_str());

			ret->spirv.resize(output.shadersize);
			memcpy(ret->spirv.data(), output.shaderdata, output.shadersize);
		}
		else
		{
			Logger::Error("Compile shader fail:%d", path.c_str());
			if (!output.errorMessage.empty())
				Logger::Error(output.errorMessage.c_str());

			variants.free(ret);
			return nullptr;
		}
	}
#endif

	ret->SetHash(hash);
	variants.insert(hash, ret);
	return ret;
}

ShaderTemplateProgram::ShaderTemplateProgram(DeviceVulkan& device_, ShaderStage stage, ShaderTemplate* shaderTemplate) :
	device(device_)
{
	SetShader(stage, shaderTemplate);
}

ShaderTemplateProgramVariant* ShaderTemplateProgram::RegisterVariant(const ShaderVariantMap& defines)
{
	HashCombiner hasher;
	for (auto& define : defines)
		hasher.HashCombine(define.c_str());

	HashValue hash = hasher.Get();
	auto it = variantCache.find(hash);
	if (it != variantCache.end())
	{
		return it->second;
	}

	ShaderTemplateProgramVariant* ret = variantCache.allocate(device);
	if (ret == nullptr)
	{
		variantCache.free(ret);
		return nullptr;
	}

	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		if (shaderTemplates[i] != nullptr)
			ret->shaderVariants[i] = shaderTemplates[i]->RegisterVariant(defines);
	}

	ret->SetHash(hash);
	variantCache.insert(hash, ret);

	return ret;
}

void ShaderTemplateProgram::SetShader(ShaderStage stage, ShaderTemplate* shader)
{
	shaderTemplates[static_cast<U32>(stage)] = shader;
}

ShaderTemplateProgramVariant::ShaderTemplateProgramVariant(DeviceVulkan& device_) :
	device(device_)
{
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		shaderInstances[i] = 0;
		shaders[i] = nullptr;
	}
}

ShaderTemplateProgramVariant::~ShaderTemplateProgramVariant()
{
}

Shader* ShaderTemplateProgramVariant::GetShader(ShaderStage stage)
{
	U32 stageIndex = static_cast<U32>(stage);
	auto variant = shaderVariants[stageIndex];
	if (variant == nullptr || variant->spirv.empty())
		return nullptr;

	Shader* ret = nullptr;
	if (shaderInstances[stageIndex] != variant->instance)
	{
		Shader* newShader = nullptr;
		if (!variant->spirv.empty())
			newShader = &device.RequestShader(stage, variant->spirv.data(), variant->spirv.size());

		if (newShader == nullptr)
			return nullptr;

		shaders[stageIndex] = newShader;
		ret = newShader;
		shaderInstances[stageIndex] = variant->instance;
	}
	else
	{
		ret = shaders[stageIndex];
	}	
	return ret;
}

ShaderProgram* ShaderTemplateProgramVariant::GetProgram()
{
	if (shaderVariants[static_cast<U32>(ShaderStage::CS)])
	{
		return GetProgramCompute();
	}
	else
	{
		bool hasShader = false;
		for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
		{
			if (i == static_cast<int>(ShaderStage::CS))
				continue;

			if (shaderVariants[i] != nullptr)
			{
				hasShader = true;
				break;
			}
		}

		if (hasShader)
			return GetProgramGraphics();
	}
	return nullptr;
}

ShaderProgram* ShaderTemplateProgramVariant::GetProgramGraphics()
{
	bool isProgramDirty = false;
	ShaderProgram* ret = nullptr;
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		if (shaderVariants[i] != nullptr)
		{
			if (shaderVariants[i]->instance != shaderInstances[i])
			{
				isProgramDirty = true;
				break;
			}
		}
	}

	if (program == nullptr)
		isProgramDirty = true;

	if (isProgramDirty == false)
		return program;

	// Request shaders
	Shader* shaders[static_cast<int>(ShaderStage::Count)] = {};
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		shaders[i] = nullptr;

		if (shaderVariants[i] != nullptr)
		{
			Shader* newShader = nullptr;
			if (!shaderVariants[i]->spirv.empty())
				newShader = &device.RequestShader(static_cast<ShaderStage>(i), shaderVariants[i]->spirv.data(), shaderVariants[i]->spirv.size());

			if (newShader == nullptr)
				return nullptr;

			shaders[i] = newShader;
		}
	}

	// Request program
	ShaderProgram* newProgram = device.RequestProgram(shaders);
	if (newProgram == nullptr)
		return nullptr;

	program = newProgram;

	// Set shader instances
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		if (shaderVariants[i] != nullptr)
			shaderInstances[i] = shaderVariants[i]->instance;
	}

	return newProgram;
}

ShaderProgram* ShaderTemplateProgramVariant::GetProgramCompute()
{
	return nullptr;
}

ShaderManager::ShaderManager(DeviceVulkan& device_) :
	device(device_)
{
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
	auto it = programs.find(hasher.Get());
	if (it != programs.end())
		return it->second;

	return programs.emplace(hasher.Get(), device, stage, templ);
}

ShaderTemplate* ShaderManager::GetTemplate(ShaderStage stage, const std::string filePath)
{
	HashValue hash = GetPathHash(stage, filePath);
	auto it = shaders.find(hash);
	if (it != shaders.end())
	{
		return it->second;
	}

	ShaderTemplate* ret = shaders.allocate(device, stage, filePath, hash);
	if (ret == nullptr || !ret->Initialize())
	{
		shaders.free(ret);
		return nullptr;
	}

	ret->SetHash(hash);
	shaders.insert(hash, ret);
	return ret;
}