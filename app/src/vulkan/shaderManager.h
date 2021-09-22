#pragma once

#include "definition.h"
#include "shader.h"

class DeviceVulkan;

using ShaderVariantMap = std::vector<std::pair<std::string, int>>;

class ShaderTemplate : public HashedObject<ShaderTemplate>
{
public:
	ShaderTemplate(DeviceVulkan& device, const std::string& path, HashValue pathHash);
	~ShaderTemplate();

	bool Initialize();

	HashValue GetPathHash()const
	{
		return mPathHash;
	}

private:
	DeviceVulkan& mDevice;
	std::string mPath;
	HashValue mPathHash;
};

class ShaderTemplateProgramVariant
{
public:
	Shader* GetShader(ShaderStage stage);

private:
	DeviceVulkan& mDevice;
};

class ShaderTemplateProgram
{
public:
	ShaderTemplateProgram(DeviceVulkan& device, ShaderStage stage, ShaderTemplate* shaderTemplate);

	ShaderTemplateProgramVariant* RegisterVariant(const ShaderVariantMap& defines);
	void SetShader(ShaderStage stage, ShaderTemplate* shader);

private:
	DeviceVulkan& mDevice;
	ShaderTemplate* mShaderTemplates[static_cast<U32>(ShaderStage::Count)] = {};
};

class ShaderManager
{
public:
	ShaderManager(DeviceVulkan& device);
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
	VulkanCache<ShaderTemplate> mShaders;
	VulkanCache<ShaderTemplateProgram> mPrograms;
	DeviceVulkan& mDevice;
};