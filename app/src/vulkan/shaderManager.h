#pragma once

#include "definition.h"
#include "shader.h"

class DeviceVulkan;

using ShaderVariantMap = std::vector<std::string>;

class ShaderTemplate : public HashedObject<ShaderTemplate>
{
public:
	ShaderTemplate(DeviceVulkan& device_, ShaderStage stage_, const std::string& path_, HashValue pathHash_);
	~ShaderTemplate();

	bool Initialize();
	
	HashValue GetPathHash()const
	{
		return pathHash;
	}

	struct Variant : public HashedObject<Variant>
	{
		HashValue hash = 0;
		HashValue spirvHash = 0;
		std::vector<uint8_t> spirv;
		ShaderVariantMap defines;
		U32 instance = 0;
	};
	Variant* RegisterVariant(const ShaderVariantMap& defines);

private:
	friend class ShaderTemplateProgram;

	DeviceVulkan& device;
	std::string path;
	HashValue pathHash;
	VulkanCache<Variant> variants;
	ShaderStage stage;
};

class ShaderTemplateProgramVariant : public HashedObject<ShaderTemplateProgramVariant>
{
public:
	ShaderTemplateProgramVariant(DeviceVulkan& device_);
	~ShaderTemplateProgramVariant();

	Shader* GetShader(ShaderStage stage);
	ShaderProgram* GetProgram();

private:
	friend class ShaderTemplateProgram;

	DeviceVulkan& device;
	const ShaderTemplate::Variant* shaderVariants[static_cast<U32>(ShaderStage::Count)] = {};
	ShaderProgram* program = nullptr;
	U32 shaderInstances[static_cast<U32>(ShaderStage::Count)] = {};
	Shader* shaders[static_cast<U32>(ShaderStage::Count)] = {};
};

class ShaderTemplateProgram
{
public:
	ShaderTemplateProgram(DeviceVulkan& device_, ShaderStage stage, ShaderTemplate* shaderTemplate);

	ShaderTemplateProgramVariant* RegisterVariant(const ShaderVariantMap& defines);
	void SetShader(ShaderStage stage, ShaderTemplate* shader);

private:
	DeviceVulkan& device;
	ShaderTemplate* shaderTemplates[static_cast<U32>(ShaderStage::Count)] = {};
	VulkanCache<ShaderTemplateProgramVariant> variantCache;
	ShaderProgram* shaderProgram = nullptr;
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
	
private:
	ShaderTemplate* GetTemplate(ShaderStage stage, const std::string filePath);

private:
	VulkanCache<ShaderTemplate> shaders;
	VulkanCache<ShaderTemplateProgram> programs;
	DeviceVulkan& device;
};