#include "shader.h"
#include "device.h"

namespace VulkanTest
{
namespace GPU
{
Shader::Shader(DeviceVulkan& device_, ShaderStage shaderStage_, VkShaderModule shaderModule_, const ShaderResourceLayout* layout_) :
	device(device_),
	shaderStage(shaderStage_),
	shaderModule(shaderModule_)
{
}

Shader::Shader(DeviceVulkan& device_, ShaderStage shaderStage_, const void* pShaderBytecode, size_t bytecodeLength, const ShaderResourceLayout* layout_):
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

	if (layout_ != nullptr)
	{
		layout = *layout_;
	}
	else if (!device.GetShaderManager().ReflectShader(layout, static_cast<const U32*>(pShaderBytecode), bytecodeLength))
	{
		Logger::Error("Failed to reflect shader resource layout.");
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
#ifdef VULKAN_MT
	for (auto it : pipelines.GetReadOnly())
		device->ReleasePipeline(it.Get());
	for (auto it : pipelines.GetReadWrite())
		device->ReleasePipeline(it.Get());
#else
	for (auto kvp : pipelines)
		device->ReleasePipeline(kvp.second);
#endif
}

void ShaderProgram::AddPipeline(HashValue hash, VkPipeline pipeline)
{
	pipelines.emplace(hash, pipeline);
}

VkPipeline ShaderProgram::GetPipeline(HashValue hash)
{
	auto it = pipelines.find(hash);
	return it != nullptr ? it->Get() : VK_NULL_HANDLE;
}

void ShaderProgram::MoveToReadOnly()
{
#ifdef VULKAN_MT
	pipelines.MoveToReadOnly();
#endif
}

void ShaderProgram::SetShader(ShaderStage stage, Shader* shader)
{
	shaders[(int)stage] = shader;
}

PipelineLayout::PipelineLayout(DeviceVulkan& device_, CombinedResourceLayout resLayout_) :
	device(device_),
	resLayout(resLayout_)
{
	// PipelineLayout = DescriptorSetLayouts + PushConstants

	U32 numSets = 0;
	VkDescriptorSetLayout layouts[VULKAN_NUM_DESCRIPTOR_SETS] = {};
	for (U32 i = 0; i < VULKAN_NUM_DESCRIPTOR_SETS; i++)
	{
		if (resLayout.descriptorSetMask & (1 << i))
		{
			auto allocator = &device.RequestDescriptorSetAllocator(resLayout_.sets[i], resLayout_.stagesForBindings[i]);
			descriptorSetAllocators[i] = allocator;
			layouts[i] = allocator->GetSetLayout();
			numSets = i + 1;
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
}