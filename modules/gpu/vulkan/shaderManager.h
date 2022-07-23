#pragma once

#include "definition.h"
#include "shader.h"
#include "core\platform\sync.h"
#include "rwSpinLock.h"

namespace VulkanTest
{
namespace GPU
{

class DeviceVulkan;

using ShaderVariantMap = std::vector<std::string>;
struct ShaderTemplateVariant : public Util::IntrusiveHashMapEnabled<ShaderTemplateVariant>
{
	HashValue hash = 0;
	HashValue spirvHash = 0;
	std::vector<uint8_t> spirv;
	ShaderVariantMap defines;
	volatile U32 instance = 0;
};

class ShaderTemplate : public Util::IntrusiveHashMapEnabled<ShaderTemplate>
{
public:
	ShaderTemplate(DeviceVulkan& device_, ShaderStage stage_, const std::string& path_, HashValue pathHash_);
	~ShaderTemplate();

	bool Initialize();
	void Recompile();

	ShaderTemplateVariant* RegisterVariant(const ShaderVariantMap& defines);

	HashValue GetPathHash()const
	{
		return pathHash;
	}

private:
	friend class ShaderTemplateProgram;

	void RecompileVariant(ShaderTemplateVariant& variant);
	bool CompileShader(ShaderTemplateVariant* variant, const ShaderVariantMap& defines);

	DeviceVulkan& device;
	std::string path;
	HashValue pathHash;
	VulkanCache<ShaderTemplateVariant> variants;
	ShaderStage stage;

#ifdef VULKAN_MT
	Mutex lock;
#endif
};

class ShaderTemplateProgramVariant : public Util::IntrusiveHashMapEnabled<ShaderTemplateProgramVariant>
{
public:
	ShaderTemplateProgramVariant(DeviceVulkan& device_);
	~ShaderTemplateProgramVariant();

	Shader* GetShader(ShaderStage stage);
	ShaderProgram* GetProgram();

private:
	friend class ShaderTemplateProgram;

	ShaderProgram* GetProgramGraphics();
	ShaderProgram* GetProgramCompute();

private:
	DeviceVulkan& device;
	// const ShaderTemplateVariant* shaderVariants[static_cast<U32>(ShaderStage::Count)] = {};
	ShaderTemplateVariant* shaderVariants[static_cast<U32>(ShaderStage::Count)] = {};
	std::atomic<ShaderProgram*> program = nullptr;
	std::atomic<U32> shaderInstances[static_cast<U32>(ShaderStage::Count)] = {};
	Shader* shaders[static_cast<U32>(ShaderStage::Count)] = {};

#ifdef VULKAN_MT
	Tools::RWSpinLock lock;
#endif
};

class ShaderTemplateProgram : public Util::IntrusiveHashMapEnabled<ShaderTemplateProgram>
{
public:
	ShaderTemplateProgram(DeviceVulkan& device_, ShaderStage stage, ShaderTemplate* shaderTemplate);
	ShaderTemplateProgram(DeviceVulkan& device_, const std::vector<std::pair<ShaderStage, ShaderTemplate*>>& templates);

	ShaderTemplateProgramVariant* RegisterVariant(const ShaderVariantMap& defines);

private:
	void SetShader(ShaderStage stage, ShaderTemplate* shader);

	DeviceVulkan& device;
	ShaderTemplate* shaderTemplates[static_cast<U32>(ShaderStage::Count)] = {};
	VulkanCacheReadWrite<ShaderTemplateProgramVariant> variantCache;

#ifdef VULKAN_MT
	Tools::RWSpinLock lock;
#endif
};

class ShaderManager
{
public:
	ShaderManager(DeviceVulkan& device_);
	~ShaderManager();

	/**
	 * 从指定路径加载一个Shader,这包含了RegisterShader的过程.
	 * @param stage shader stage
	 * @param filePath shader的目标路径
	 * @param defines shader设置的宏定义
	 * @return Shader 返回一个compiled shader实例
	 */
	Shader* LoadShader(ShaderStage stage, const std::string& filePath, const ShaderVariantMap& defines);
	
	/**
	 * 从指定路径注册一个Shader，这将创建一个未编译的shader template.
	 * @param stage shader stage
	 * @param filePath shader的目标路径
	 * @return ShaderTemplateProgram 返回未编译的shaderTemplate
	 */
	ShaderTemplateProgram* RegisterShader(ShaderStage stage, const std::string& filePath);

	/**
	 * 注册一个Program
	 */
	ShaderTemplateProgram* RegisterGraphics(const std::string& vertex, const std::string& fragment, const ShaderVariantMap& defines);

	bool LoadShaderCache(const char* path);
	void MoveToReadOnly();

private:
	ShaderTemplate* GetTemplate(ShaderStage stage, const std::string filePath);

private:
	VulkanCache<ShaderTemplate> shaders;
	VulkanCache<ShaderTemplateProgram> programs;
	DeviceVulkan& device;

#ifdef VULKAN_MT
	Mutex lock;
#endif
};

}
}