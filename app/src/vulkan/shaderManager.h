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
	VulkanCache<ShaderTemplate> mShaders;
	VulkanCache<ShaderTemplateProgram> mPrograms;
	DeviceVulkan& mDevice;
};