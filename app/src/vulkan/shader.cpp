#include "shader.h"
#include "device.h"

Shader::Shader(DeviceVulkan& device, ShaderStage shaderStage, VkShaderModule shaderModule) :
	mDevice(device),
	mShaderStage(shaderStage),
	mShaderModule(shaderModule)
{
}

Shader::Shader(DeviceVulkan& device, ShaderStage shaderStage, const void* pShaderBytecode, size_t bytecodeLength):
	mDevice(device),
	mShaderStage(shaderStage)
{
	VkShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = bytecodeLength;
	moduleInfo.pCode = (const uint32_t*)pShaderBytecode;

	if (vkCreateShaderModule(mDevice.mDevice, &moduleInfo, nullptr, &mShaderModule) != VK_SUCCESS)
	{
		Logger::Error("Failed to create shader.");
		return;
	}
}

Shader::~Shader()
{
	if (mShaderModule != VK_NULL_HANDLE)
		vkDestroyShaderModule(mDevice.mDevice, mShaderModule, nullptr);
}

ShaderProgram::ShaderProgram(DeviceVulkan* device, const ShaderProgramInfo& info) :
	mDevice(device)
{
	if (info.VS != nullptr)
		SetShader(ShaderStage::VS, info.VS);
	if (info.PS != nullptr)
		SetShader(ShaderStage::PS, info.PS);

	device->BakeShaderProgram(*this);
}

ShaderProgram::~ShaderProgram()
{
	for (auto& kvp : mPipelines)
		mDevice->ReleasePipeline(kvp.second);
}

void ShaderProgram::AddPipeline(HashValue hash, VkPipeline pipeline)
{
	mPipelines.try_emplace(hash, pipeline);
}

VkPipeline ShaderProgram::GetPipeline(HashValue hash)
{
	auto it = mPipelines.find(hash);
	return it != mPipelines.end() ? it->second : VK_NULL_HANDLE;
}

void ShaderProgram::SetShader(ShaderStage stage, Shader* shader)
{
	mShaders[(int)stage] = shader;
}

PipelineLayout::PipelineLayout(DeviceVulkan& device) :
	mDevice(device)
{
}

PipelineLayout::~PipelineLayout()
{
}