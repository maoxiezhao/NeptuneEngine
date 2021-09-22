#pragma once

#include "definition.h"
#include "shader.h"

class DeviceVulkan;

using ShaderVariantMap = std::vector<std::pair<std::string, int>>;

class ShaderTemplate : public HashedObject<ShaderTemplate>
{
public:
	ShaderTemplate(DeviceVulkan& device_, const std::string& path_, HashValue pathHash_);
	~ShaderTemplate();

	bool Initialize();
	
	HashValue GetPathHash()const
	{
		return pathHash;
	}

private:
	friend class ShaderTemplateProgram;

	struct Variant
	{
		 
	};
	Variant* RegisterVariant(const ShaderVariantMap& defines);

private:
	DeviceVulkan& device;
	std::string path;
	HashValue pathHash;
};

class ShaderTemplateProgramVariant
{
public:
	Shader* GetShader(ShaderStage stage);

private:
	DeviceVulkan& device;
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