#include "shader.h"
#include "device.h"

Shader::Shader(DeviceVulkan& device_, ShaderStage shaderStage_, VkShaderModule shaderModule_) :
	device(device_),
	shaderStage(shaderStage_),
	shaderModule(shaderModule_)
{
}

Shader::Shader(DeviceVulkan& device_, ShaderStage shaderStage_, const void* pShaderBytecode, size_t bytecodeLength):
	device(device_),
	shaderStage(shaderStage_)
{
	VkShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = bytecodeLength;
	moduleInfo.pCode = (const uint32_t*)pShaderBytecode;

	if (vkCreateShaderModule(device.device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		Logger::Error("Failed to create shader.");
		return;
	}
}

Shader::~Shader()
{
	if (shaderModule != VK_NULL_HANDLE)
		vkDestroyShaderModule(device.device, shaderModule, nullptr);
}

ShaderProgram::ShaderProgram(DeviceVulkan* device_, const ShaderProgramInfo& info) :
	device(device_)
{
	if (info.vs != nullptr)
		SetShader(ShaderStage::VS, info.vs);
	if (info.ps != nullptr)
		SetShader(ShaderStage::PS, info.ps);

	device->BakeShaderProgram(*this);
}

ShaderProgram::~ShaderProgram()
{
	for (auto& kvp : pipelines)
		device->ReleasePipeline(kvp.second);
}

void ShaderProgram::AddPipeline(HashValue hash, VkPipeline pipeline)
{
	pipelines.try_emplace(hash, pipeline);
}

VkPipeline ShaderProgram::GetPipeline(HashValue hash)
{
	auto it = pipelines.find(hash);
	return it != pipelines.end() ? it->second : VK_NULL_HANDLE;
}

void ShaderProgram::SetShader(ShaderStage stage, Shader* shader)
{
	shaders[(int)stage] = shader;
}

PipelineLayout::PipelineLayout(DeviceVulkan& device_) :
	device(device_)
{
}

PipelineLayout::~PipelineLayout()
{
}