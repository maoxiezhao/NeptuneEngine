#include "shaderManager.h"
#include "shaderCompiler.h"
#include "device.h"
#include "core\platform\atomic.h"
#include "core\utils\archive.h"
#include "core\utils\path.h"
#include "core\utils\helper.h"
#include "core\utils\string.h"
#include "core\filesystem\filesystem.h"

#include <unordered_set>
#include <filesystem>

namespace VulkanTest
{
namespace GPU
{

using namespace ShaderCompiler;

#define RUNTIME_SHADERCOMPILER_ENABLED

// Shader flow:
// ShaderManager -> ShaderTemplateProgram -> ShaderTemplateProgramVariant  -> ShaderTemplate -> Variant -> Shader

const std::string ShaderManager::EXPORT_SHADER_PATH = ".export/shaders/";
const std::string ShaderManager::SOURCE_SHADER_PATH = "shaders/";

namespace {

	std::mutex locker;
	std::unordered_set<std::string> registeredShaders;
	std::unordered_map<HashValue, HashValue> shaderCache;

	HashValue GetPathHash(ShaderStage stage, const char* path)
	{
		HashCombiner hasher;
		hasher.HashCombine(static_cast<U32>(stage));
		hasher.HashCombine(path);
		return hasher.Get();
	}

	void RegisterShader(const std::string& shaderfilename)
	{
#ifdef RUNTIME_SHADERCOMPILER_ENABLED
		std::scoped_lock lock(locker);
		registeredShaders.insert(shaderfilename);
#endif
	}

	bool IsShaderOutdated(FileSystem& fs, const char* shaderfilename)
	{
#ifdef RUNTIME_SHADERCOMPILER_ENABLED
		if (!fs.FileExists(shaderfilename))
			return true;

		std::string metaPath = shaderfilename;
		metaPath = Helper::ReplaceExtension(metaPath, "shadermeta");
		if (!fs.FileExists(metaPath.c_str()))
			return false;

		const U64 lastModeTime = fs.GetLastModTime(shaderfilename);
		Archive dependencyLibrary(metaPath);
		if (dependencyLibrary.IsOpen())
		{
			std::string rootdir = dependencyLibrary.GetSourceDirectory();
			std::vector<std::string> dependencies;
			dependencyLibrary >> dependencies;

			for (auto& x : dependencies)
			{
				std::string dependencypath = rootdir + x;
				if (fs.FileExists(dependencypath.c_str()))
				{
					const U64 depModTime = fs.GetLastModTime(dependencypath.c_str());
					if (lastModeTime < depModTime)
						return true;
				}
			}
		}
#endif
		return false;
	}

	bool SaveShaderAndMetadata(FileSystem& fs, const char* shaderfilename, const CompilerOutput& output)
	{
#ifdef RUNTIME_SHADERCOMPILER_ENABLED
		if (!Platform::DirExists(ShaderManager::EXPORT_SHADER_PATH.c_str()))
		{
			if (!Platform::MakeDir(ShaderManager::EXPORT_SHADER_PATH.c_str()))
			{
				Logger::Error("Failed to create the directory of export shader:%s", shaderfilename);
				return false;
			}
		}

		std::string metaPath = shaderfilename;
		metaPath = Helper::ReplaceExtension(metaPath, "shadermeta");
		Archive dependencyLibrary(metaPath, false);
		if (dependencyLibrary.IsOpen())
		{
			std::string rootdir = dependencyLibrary.GetSourceDirectory();
			std::vector<std::string> dependencies = output.dependencies;
			for (auto& x : dependencies)
				Helper::MakePathRelative(rootdir, x);

			dependencyLibrary << dependencies;
		}

		auto file = fs.OpenFile(shaderfilename, FileFlags::DEFAULT_WRITE);
		if (!file->IsValid())
		{
			Logger::Error("Failed to save shader metadata.");
			return false;
		}
		file->Write(output.shaderdata, output.shadersize);
		file->Close();

#endif // RUNTIME_SHADERCOMPILER_ENABLED

		return true;
	}
}

ShaderTemplate::ShaderTemplate(DeviceVulkan& device_, ShaderStage stage_, const std::string& path_, HashValue pathHash_) :
	device(device_),
	stage(stage_),
	path(path_),
	pathHash(pathHash_),
	sourceData(nullptr),
	sourceSize(0)
{
}

ShaderTemplate::ShaderTemplate(DeviceVulkan& device_, ShaderStage stage_, HashValue pathHash_, char* data_, U32 size_) :
	device(device_),
	stage(stage_),
	pathHash(pathHash_),
	sourceData(data_),
	sourceSize(size_)
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

void ShaderTemplate::Recompile()
{	
	for (auto& variant : variants.GetReadOnly())
		RecompileVariant(variant);
	for (auto& variant : variants.GetReadWrite())
		RecompileVariant(variant);
}

ShaderTemplateVariant* ShaderTemplate::RegisterVariant(const ShaderVariantMap& defines)
{
	HashCombiner hasher;
	for (auto& define : defines)
	{
		hasher.HashCombine(define.name.c_str());
		hasher.HashCombine(define.definition);
	}

	HashValue hash = hasher.Get();
	auto it = variants.find(hash);
	if (it != nullptr)
		return it;

	hasher.HashCombine(pathHash);
	HashValue completeHash = hasher.Get();

	ShaderTemplateVariant* ret = variants.allocate();
	ret->hash = completeHash;
	if (!CompileShader(ret, defines))
		return nullptr;

	ret->instance++;
	ret->SetHash(hash);

	if (!defines.empty())
		ret->defines = defines;

	variants.insert(hash, ret);
	return ret;
}

void ShaderTemplate::RecompileVariant(ShaderTemplateVariant& variant)
{
	if (!CompileShader(&variant, variant.defines))
	{
		Logger::Error("Failed to compile shader:%s", path.c_str());
		return;
	}

	variant.instance++;
}

bool ShaderTemplate::CompileShader(ShaderTemplateVariant* variant, const ShaderVariantMap& defines)
{
	auto& fs = *device.GetSystemHandles().fileSystem;

#ifdef RUNTIME_SHADERCOMPILER_ENABLED
	{		
		ScopedMutex holder(lock);
		std::string exportShaderPath = ShaderManager::EXPORT_SHADER_PATH + std::to_string(variant->hash) + ".shader";
		RegisterShader(exportShaderPath);

		if (IsShaderOutdated(fs, exportShaderPath.c_str()))
		{
			String msg = String("Compiling shader:") + path.c_str();
			if (!defines.empty())
				msg += "\n\tDefines:\n";
			for (const auto& define : defines)
			{
				msg += "\t\t";
				msg += define.name.c_str();
				msg += "=";
				msg += std::to_string(define.definition).c_str();
				msg += "\n";
			}
			Logger::Info(msg.c_str());

			std::string sourcedir = ShaderManager::SOURCE_SHADER_PATH;
			Helper::MakePathAbsolute(sourcedir);

			CompilerInput input;
			input.stage = stage;
			input.includeDirectories.push_back(sourcedir);
			input.defines = defines;

			if (!path.empty() && sourceData == nullptr && sourceSize == 0)
			{
				input.shadersourcefilename = Helper::ReplaceExtension(sourcedir + path, "hlsl");
			}
			else
			{
				input.shadersourceData = sourceData;
				input.shadersourceSize = sourceSize;
			}
	
			CompilerOutput output;
			if (Compile(input, output))
			{
				Logger::Info("Compile shader successfully:%s", path.c_str());
				SaveShaderAndMetadata(fs, exportShaderPath.c_str(), output);
				if (!output.errorMessage.empty())
					Logger::Error(output.errorMessage.c_str());

				variant->spirv.resize(output.shadersize);
				memcpy(variant->spirv.data(), output.shaderdata, output.shadersize);
			}
			else
			{
				Logger::Error("Compile shader fail:%d", path.c_str());
				if (!output.errorMessage.empty())
					Logger::Error(output.errorMessage.c_str());

				variants.free(variant);
				return false;
			}
		}
		else
		{
			OutputMemoryStream mem;
			if (!fs.LoadContext(exportShaderPath.c_str(), mem))
			{
				Logger::Error("Failed to load export shader:%s", exportShaderPath.c_str());
				return false;
			}

			variant->spirv.resize(mem.Size());
			memcpy(variant->spirv.data(), mem.Data(), mem.Size());
		}
	}
	return true;
#else
	ASSERT(false);
	return nullptr;
#endif
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
	{
		hasher.HashCombine(define.name.c_str());
		hasher.HashCombine(define.definition);
	}

	HashValue hash = hasher.Get();
	
	lock.BeginRead();
	auto it = variantCache.find(hash);
	if (it != nullptr)
		return it;
	lock.EndRead();

	lock.BeginWrite();
	ShaderTemplateProgramVariant* newVariant = variantCache.allocate(device);
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		if (shaderTemplates[i] != nullptr)
			newVariant->shaderVariants[i] = shaderTemplates[i]->RegisterVariant(defines);
	}
	// newVariant->GetProgram();
	newVariant->SetHash(hash);
	variantCache.insert(hash, newVariant);
	lock.EndWrite();

	return newVariant;
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
		shaderInstances[i].store(0, std::memory_order_relaxed);
		shaders[i] = nullptr;
	}
	program.store(nullptr, std::memory_order_relaxed);
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
		{
			newShader = device.RequestShader(
				stage,
				variant->spirv.data(),
				variant->spirv.size(),
				nullptr);

#if 0
			// TODO: TEMP
			if (newShader && newShader->GetHash() != 0)
			{
				variant->spirvHash = newShader->GetHash();
				variant->spirv.clear();
				ASSERT(variant->spirvHash);
			}
#endif
		}
		else
		{
			ASSERT(variant->spirvHash);
			newShader = device.RequestShaderByHash(variant->spirvHash);
		}

		shaders[stageIndex] = newShader;
		ret = newShader;
		shaderInstances[stageIndex] = variant->instance;
	}
	else
	{
		ret = shaders[stageIndex];
	}	

#ifdef VULKAN_MT
	lock.EndWrite();
#endif

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
			if (shaderVariants[i]->instance != shaderInstances[i].load(std::memory_order_acquire))
			{
				isProgramDirty = true;
				break;
			}
		}
	}
	if (isProgramDirty == false)
		return program.load(std::memory_order_relaxed);

	isProgramDirty = false;
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		if (shaderVariants[i] != nullptr)
		{
			if (shaderVariants[i]->instance != shaderInstances[i].load(std::memory_order_relaxed))
			{
				isProgramDirty = true;
				break;
			}
		}
	}
	if (isProgramDirty == false)
		return program.load(std::memory_order_relaxed);

#ifdef VULKAN_MT
	lock.BeginWrite();
#endif

	// Request shaders
	const Shader* shaders[static_cast<int>(ShaderStage::Count)] = {};
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		shaders[i] = nullptr;

		if (shaderVariants[i] != nullptr)
		{
			Shader* newShader = nullptr;
			if (!shaderVariants[i]->spirv.empty())
			{
				newShader = device.RequestShader(
					static_cast<ShaderStage>(i),
					shaderVariants[i]->spirv.data(),
					shaderVariants[i]->spirv.size(),
					nullptr);			
#if 0
				// TODO: TEMP
				if (newShader && newShader->GetHash() != 0)
				{
					shaderVariants[i]->spirvHash = newShader->GetHash();
					shaderVariants[i]->spirv.clear();
					ASSERT(shaderVariants[i]->spirvHash);
				}
#endif
			}
			else
			{
				ASSERT(shaderVariants[i]->spirvHash);
				newShader = device.RequestShaderByHash(shaderVariants[i]->spirvHash);
			}

			shaders[i] = newShader;
		}
	}
#ifdef VULKAN_MT
	lock.EndWrite();
#endif

	// Request program
	ShaderProgram* newProgram = device.RequestProgram(shaders);
	if (newProgram != nullptr)
	{
		program.store(newProgram, std::memory_order_relaxed);

		// Set shader instances
		for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
		{
			if (shaderVariants[i] != nullptr)
				shaderInstances[i].store(shaderVariants[i]->instance, std::memory_order_release);
		}
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

ShaderTemplateProgram* ShaderManager::RegisterGraphics(const std::string& vertex, const std::string& fragment)
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

ShaderTemplateProgram* ShaderManager::RegisterCompute(const std::string& comptue)
{
	ShaderTemplate* computeTempl = GetTemplate(ShaderStage::CS, comptue);
	if (computeTempl == nullptr)
		return nullptr;

	HashCombiner hasher;
	hasher.HashCombine(computeTempl->GetPathHash());
	auto it = programs.find(hasher.Get());
	if (it != nullptr)
		return it;

	std::vector<std::pair<ShaderStage, ShaderTemplate*>> templates;
	templates.push_back(std::make_pair(ShaderStage::CS, computeTempl));
	return programs.emplace(hasher.Get(), device, templates);
}

ShaderTemplate* ShaderManager::GetTemplate(ShaderStage stage, const std::string filePath)
{
	HashValue hash = GetPathHash(stage, filePath.c_str());
	ShaderTemplate* temp = shaders.find(hash);
	if (temp == nullptr)
	{
		ShaderTemplate* shader = shaders.allocate(device, stage, filePath, hash);
		if (!shader->Initialize())
		{
			shaders.free(shader);
			return nullptr;
		}
		temp = shaders.insert(hash, shader);
	}
	return temp;
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