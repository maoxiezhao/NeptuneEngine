#include "shaderManager.h"
#include "shaderCompiler.h"
#include "device.h"
#include "core\utils\archive.h"
#include "core\utils\helper.h"
#include "spriv_reflect\spirv_reflect.h"

#include <unordered_set>
#include <filesystem>

namespace VulkanTest
{
namespace GPU
{

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

ShaderTemplateVariant* ShaderTemplate::RegisterVariant(const ShaderVariantMap& defines)
{
	HashCombiner hasher;
	for (auto& define : defines)
		hasher.HashCombine(define.c_str());

	HashValue hash = hasher.Get();
	auto it = variants.find(hash);
	if (it != nullptr)
		return it;

	ShaderTemplateVariant* ret = variants.allocate();
	if (ret == nullptr)
	{
		variants.free(ret);
		return nullptr;
	}

	hasher.HashCombine(pathHash);
	ret->hash = hasher.Get();

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
			Logger::Info("Compile shader successfully:%d", path.c_str());
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
	else
	{
		if (!Helper::FileRead(exportShaderPath, ret->spirv))
		{
			Logger::Error("Failed to load export shader:%s", exportShaderPath.c_str());
			return nullptr;
		}
	}
#endif
	ret->instance++;
	ret->SetHash(hash);

	if (!defines.empty())
		ret->defines = defines;

	variants.insert(hash, ret);
	return ret;
}

ShaderTemplateProgram::ShaderTemplateProgram(DeviceVulkan& device_, ShaderStage stage, ShaderTemplate* shaderTemplate) :
	device(device_)
{
	SetShader(stage, shaderTemplate);
}

ShaderTemplateProgram::ShaderTemplateProgram(DeviceVulkan& device_, const std::vector<std::pair<ShaderStage, ShaderTemplate*>>& templates) :
	device(device_)
{
	for (auto kvp : templates)
		SetShader(kvp.first, kvp.second);
}

ShaderTemplateProgramVariant* ShaderTemplateProgram::RegisterVariant(const ShaderVariantMap& defines)
{
	HashCombiner hasher;
	for (auto& define : defines)
		hasher.HashCombine(define.c_str());

	HashValue hash = hasher.Get();
	auto it = variantCache.find(hash);
	if (it != nullptr)
		return it;

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
			newShader = &device.RequestShader(
				stage,
				variant->spirv.data(), 
				variant->spirv.size(),
				nullptr);

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
	// Check shader program is dirty
	bool isProgramDirty = false;
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

#ifdef VULKAN_MT
	lock.BeginWrite();
#endif
	// Request shaders
	Shader* shaders[static_cast<int>(ShaderStage::Count)] = {};
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		shaders[i] = nullptr;

		if (shaderVariants[i] != nullptr)
		{
			Shader* newShader = nullptr;
			if (!shaderVariants[i]->spirv.empty())
				newShader = &device.RequestShader(
					static_cast<ShaderStage>(i), 
					shaderVariants[i]->spirv.data(), 
					shaderVariants[i]->spirv.size(),
					nullptr);

			if (newShader == nullptr)
				return nullptr;

			shaders[i] = newShader;
		}
	}

	// Request program
	ShaderProgram* newProgram = device.RequestProgram(shaders);
	if (newProgram != nullptr)
	{
		program = newProgram;

		// Set shader instances
		for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
		{
			if (shaderVariants[i] != nullptr)
				shaderInstances[i] = shaderVariants[i]->instance;
		}
	}

#ifdef VULKAN_MT
	lock.EndWrite();
#endif
	return newProgram;
}

ShaderProgram* ShaderTemplateProgramVariant::GetProgramCompute()
{
	return nullptr;
}

ShaderManager::ShaderManager(DeviceVulkan& device_) :
	device(device_)
{
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
	if (it != nullptr)
		return it;

	return programs.emplace(hasher.Get(), device, stage, templ);
}

ShaderTemplateProgram* ShaderManager::RegisterGraphics(const std::string& vertex, const std::string& fragment, const ShaderVariantMap& defines)
{
	ShaderTemplate* vertTempl = GetTemplate(ShaderStage::VS, vertex);
	ShaderTemplate* fragTempl = GetTemplate(ShaderStage::PS, fragment);
	if (vertTempl == nullptr || fragTempl == nullptr)
		return nullptr;

	HashCombiner hasher;
	hasher.HashCombine(vertTempl->GetPathHash());
	hasher.HashCombine(fragTempl->GetPathHash());
	auto it = programs.find(hasher.Get());
	if (it != nullptr)
		return it;

	std::vector<std::pair<ShaderStage, ShaderTemplate*>> templates;
	templates.push_back(std::make_pair(ShaderStage::VS, vertTempl));
	templates.push_back(std::make_pair(ShaderStage::PS, fragTempl));
	return programs.emplace(hasher.Get(), device, templates);
}

ShaderTemplate* ShaderManager::GetTemplate(ShaderStage stage, const std::string filePath)
{
	HashValue hash = GetPathHash(stage, filePath);
	auto it = shaders.find(hash);
	if (it != nullptr)
		return it;

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

namespace {

	I32 GetMaskByDescriptorType(SpvReflectDescriptorType descriptorType)
	{
		I32 mask = -1;
		switch (descriptorType)
		{
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
			mask = static_cast<I32>(DescriptorSetLayout::SAMPLER);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			mask = static_cast<I32>(DescriptorSetLayout::SAMPLED_IMAGE);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			mask = static_cast<I32>(DescriptorSetLayout::SAMPLED_BUFFER);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			mask = static_cast<I32>(DescriptorSetLayout::STORAGE_BUFFER);
			break;
		default:
			break;
		}
		return mask;
	}

	void UpdateShaderArrayInfo(ShaderResourceLayout& layout, U32 set, U32 binding, SpvReflectDescriptorBinding* spvBinding)
	{
		auto& size = layout.sets[set].arraySize[binding];
		if (spvBinding->type_description->op == SpvOpTypeRuntimeArray)
		{
			size = DescriptorSetLayout::UNSIZED_ARRAY;
			layout.bindlessDescriptorSetMask |= 1u << set;
		}
		else
		{
			size = spvBinding->count;
		}
	}
}

bool ShaderManager::ReflectShader(ShaderResourceLayout& layout, const U32* spirvData, size_t spirvSize)
{
	SpvReflectShaderModule module;
	if (spvReflectCreateShaderModule(spirvSize, spirvData, &module) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to create reflect shader module.");
		return false;
	}

	// get bindings info
	U32 bindingCount = 0;
	if (spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr))
	{
		Logger::Error("Failed to reflect bindings.");
		return false;
	}
	std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
	if (spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data()))
	{
		Logger::Error("Failed to reflect bindings.");
		return false;
	}

	// get push constants info
	U32 pushCount = 0;
	if (spvReflectEnumeratePushConstantBlocks(&module, &pushCount, nullptr))
	{
		Logger::Error("Failed to reflect push constant blocks.");
		return false;
	}
	std::vector<SpvReflectBlockVariable*> pushConstants(pushCount);
	if (spvReflectEnumeratePushConstantBlocks(&module, &pushCount, pushConstants.data()))
	{
		Logger::Error("Failed to reflect push constant blocks.");
		return false;
	}

	// get input variables
	U32 inputVariableCount = 0;
	if (spvReflectEnumerateInputVariables(&module, &inputVariableCount, nullptr))
	{
		Logger::Error("Failed to reflect input variables.");
		return false;
	}
	std::vector<SpvReflectInterfaceVariable*> inputVariables(inputVariableCount);
	if (spvReflectEnumerateInputVariables(&module, &inputVariableCount, inputVariables.data()))
	{
		Logger::Error("Failed to reflect input variables.");
		return false;
	}

	// parse push constant buffers
	if (!pushConstants.empty())
	{
		// 这里仅仅获取第一个constant的大小
		// At least on older validation layers, it did not do a static analysis to determine similar information
		layout.pushConstantSize = pushConstants.front()->offset + pushConstants.front()->size;
	}

	// parse bindings
	for (auto& x : bindings)
	{
		I32 mask = GetMaskByDescriptorType(x->descriptor_type);
		if (mask >= 0)
		{
			layout.sets[x->set].masks[mask] |= 1u << x->binding;
			UpdateShaderArrayInfo(layout, x->set, x->binding, x);
		}
	}

	// parse input
	for (auto& x : inputVariables)
	{
		if (x->location <= 32)
			layout.inputMask |= 1u << x->location;
	}

	return true;
}

bool ShaderManager::LoadShaderCache(const char* path)
{
	return false;
}

void ShaderManager::MoveToReadOnly()
{
#ifdef VULKAN_MT
	shaders.MoveToReadOnly();
	programs.MoveToReadOnly();
#endif
}

}
}