#include "shader.h"
#include "device.h"

namespace GPU
{

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
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		if (info.shaders[i] != nullptr)
			SetShader(static_cast<ShaderStage>(i), info.shaders[i]);
	}

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

PipelineLayout::PipelineLayout(DeviceVulkan& device_, ResourceLayout resLayout) :
	device(device_)
{
	// PipelineLayout = DescriptorSetLayouts + PushConstants

	U32 numSets = 0;
	VkDescriptorSetLayout layouts[VULKAN_NUM_DESCRIPTOR_SETS] = {};
	for (U32 i = 0; i < VULKAN_NUM_DESCRIPTOR_SETS; i++)
	{
		if (resLayout.descriptorSetMask & (1 << i))
		{
			auto allocator = &device.RequestDescriptorSetAllocator();
			descriptorSetAllocators[i] = allocator;
			descriptorLayouts[i] = allocator->GetSetLayout();
			numSets++;
		}
	}

	VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	if (numSets > 0)
	{
		info.setLayoutCount = numSets;
		info.pSetLayouts = layouts;
	}

	if (resLayout.pushConstantRange.stageFlags != 0)
	{
		info.pushConstantRangeCount = 1;
		info.pPushConstantRanges = &resLayout.pushConstantRange;
	}

	if (vkCreatePipelineLayout(device.device, &info, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		Logger::Error("Failed to create pipeline layout.");
		return;
	}
}

PipelineLayout::~PipelineLayout()
{
	if (pipelineLayout != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(device.device, pipelineLayout, nullptr);
}

}