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
	 * ��ָ��·������һ��Shader,�������RegisterShader�Ĺ���.
	 * @param stage shader stage
	 * @param filePath shader��Ŀ��·��
	 * @param defines shader���õĺ궨��
	 * @return Shader ����һ��compiled shaderʵ��
	 */
	Shader* LoadShader(ShaderStage stage, const std::string& filePath, const ShaderVariantMap& defines);
	
	/**
	 * ��ָ��·��ע��һ��Shader���⽫����һ��δ�����shader template.
	 * @param stage shader stage
	 * @param filePath shader��Ŀ��·��
	 * @return ShaderTemplateProgram ����δ�����shaderTemplate
	 */
	ShaderTemplateProgram* RegisterShader(ShaderStage stage, const std::string& filePath);
	
private:
	ShaderTemplate* GetTemplate(ShaderStage stage, const std::string filePath);

private:
	VulkanCache<ShaderTemplate> shaders;
	VulkanCache<ShaderTemplateProgram> programs;
	DeviceVulkan& device;
};