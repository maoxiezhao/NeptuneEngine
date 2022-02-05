#include "shader.h"
#include "device.h"
#include "spriv_reflect\spirv_reflect.h"

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
	else if (!ReflectShader(layout, static_cast<const U32*>(pShaderBytecode), bytecodeLength))
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

namespace
{
	I32 GetMaskByDescriptorType(SpvReflectDescriptorType descriptorType)
	{
		I32 mask = -1;
		switch (descriptorType)
		{
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
			mask = static_cast<I32>(DescriptorSetLayout::SAMPLER);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			mask = static_cast<I32>(DescriptorSetLayout::SAMPLED_IMAGE);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			mask = static_cast<I32>(DescriptorSetLayout::SAMPLED_BUFFER);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			mask = static_cast<I32>(DescriptorSetLayout::STORAGE_BUFFER);
			break;
		default:
			break;
		}
		return mask;
	}

	void UpdateShaderArrayInfo(ShaderResourceLayout& layout, U32 set, U32 binding, SpvReflectDescriptorBinding* spvBinding)
	{
		auto& size = layout.sets[set].arraySize[binding];
		if (spvBinding->type_description->op == SpvOpTypeRuntimeArray)
		{
			size = DescriptorSetLayout::UNSIZED_ARRAY;
			layout.bindlessDescriptorSetMask |= 1u << set;
		}
		else
		{
			size = spvBinding->count;
		}
	}
}

bool Shader::ReflectShader(ShaderResourceLayout& layout, const U32* spirvData, size_t spirvSize)
{
	SpvReflectShaderModule module;
	if (spvReflectCreateShaderModule(spirvSize, spirvData, &module) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to create reflect shader module.");
		return false;
	}

	// get bindings info
	U32 bindingCount = 0;
	if (spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to reflect bindings.");
		return false;
	}
	std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
	if (spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data()) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to reflect bindings.");
		return false;
	}

	// get push constants info
	U32 pushCount = 0;
	if (spvReflectEnumeratePushConstantBlocks(&module, &pushCount, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to reflect push constant blocks.");
		return false;
	}
	std::vector<SpvReflectBlockVariable*> pushConstants(pushCount);
	if (spvReflectEnumeratePushConstantBlocks(&module, &pushCount, pushConstants.data()) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to reflect push constant blocks.");
		return false;
	}

	// get input variables
	U32 inputVariableCount = 0;
	if (spvReflectEnumerateInputVariables(&module, &inputVariableCount, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to reflect input variables.");
		return false;
	}
	std::vector<SpvReflectInterfaceVariable*> inputVariables(inputVariableCount);
	if (spvReflectEnumerateInputVariables(&module, &inputVariableCount, inputVariables.data()) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to reflect input variables.");
		return false;
	}

	// get output variables
	U32 outputVariableCount = 0;
	if (spvReflectEnumerateOutputVariables(&module, &outputVariableCount, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to reflect output variables.");
		return false;
	}
	std::vector<SpvReflectInterfaceVariable*> outputVariables;
	if (outputVariableCount > 0)
	{
		outputVariables.resize(outputVariableCount);
		if (spvReflectEnumerateOutputVariables(&module, &outputVariableCount, outputVariables.data()) != SPV_REFLECT_RESULT_SUCCESS)
		{
			Logger::Error("Failed to reflect output variables.");
			return false;
		}
	}

	// parse push constant buffers
	if (!pushConstants.empty())
	{
		// 这里仅仅获取第一个constant的大小
		// At least on older validation layers, it did not do a static analysis to determine similar information
		layout.pushConstantSize = pushConstants.front()->offset + pushConstants.front()->size;
	}

	// parse bindings
	for (auto& x : bindings)
	{
		I32 mask = GetMaskByDescriptorType(x->descriptor_type);
		if (mask >= 0)
		{
			layout.sets[x->set].masks[mask] |= 1u << x->binding;
			UpdateShaderArrayInfo(layout, x->set, x->binding, x);
		}
	}

	// parse input
	for (auto& x : inputVariables)
	{
		if (x->location <= 32)
			layout.inputMask |= 1u << x->location;
	}
	
	// parse output
	for (auto& x : outputVariables)
	{
		if (x->location <= 32)
			layout.outputMask |= 1u << x->location;
	}

	return true;
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