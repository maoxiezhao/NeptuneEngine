#include "shader.h"
#include "device.h"
#include "spriv_reflect\spirv_reflect.h"

namespace VulkanTest
{
namespace GPU
{

static U32 GetStageMaskFromShaderStage(ShaderStage stage)
{
	U32 mask = 0;
	switch (stage)
	{
	case ShaderStage::VS:
		mask = (U32)VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
		break;
	case ShaderStage::PS:
		mask = (U32)VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
		break;
	case ShaderStage::CS:
		mask = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		break;
	default:
		break;
	}
	return mask;
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
			mask = static_cast<I32>(DESCRIPTOR_SET_TYPE_SAMPLER);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			mask = static_cast<I32>(DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			mask = static_cast<I32>(DESCRIPTOR_SET_TYPE_STORAGE_IMAGE);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			mask = static_cast<I32>(DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER);
			break;
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			mask = static_cast<I32>(DESCRIPTOR_SET_TYPE_STORAGE_BUFFER);
			break;
		default:
			break;
		}
		return mask;
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
		// At least on older validation layers, it did not do a static analysis to determine similar information
		layout.pushConstantSize = pushConstants.front()->offset + pushConstants.front()->size;
	}

	// parse bindings
	for (auto& x : bindings)
	{
		I32 mask = GetMaskByDescriptorType(x->descriptor_type);
		if (mask >= 0)
		{
			U32 rolledBinding = GetRolledBinding(x->binding);
			
			// Check is immutable sampler
			if (mask == DESCRIPTOR_SET_TYPE_SAMPLER)
			{
				if (rolledBinding >= DeviceVulkan::IMMUTABLE_SAMPLER_SLOT_BEGIN)
				{
					rolledBinding -= DeviceVulkan::IMMUTABLE_SAMPLER_SLOT_BEGIN;
					layout.sets[x->set].immutableSamplerMask |= 1u << rolledBinding;
					layout.sets[x->set].immutableSamplerBindings[rolledBinding] = x->binding;
					continue;
				}
			}
	
			layout.sets[x->set].masks[mask] |= 1u << rolledBinding;
		
			auto& arraySize = layout.sets[x->set].arraySize[mask][rolledBinding];
			if (x->type_description->op == SpvOpTypeRuntimeArray)
			{
				// Bindless array
				arraySize = DescriptorSetLayout::UNSIZED_ARRAY;
				layout.bindlessDescriptorSetMask |= 1u << x->set;
			}
			else
			{
				arraySize = x->count;
			}
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

void Shader::SetEntryPoint(const char* entryPoint_)
{
	entryPoint = entryPoint_;
}

const char* Shader::GetEntryPoint() const
{
	return entryPoint.empty() ? "main" : entryPoint.c_str();
}

void ShaderProgramDeleter::operator()(ShaderProgram* owner)
{
	owner->device->programs.free(owner);
}

ShaderProgram::ShaderProgram(DeviceVulkan* device_, const ShaderProgramInfo& info) :
	device(device_)
{
	for (int i = 0; i < static_cast<int>(ShaderStage::Count); i++)
	{
		if (info.shaders[i] != nullptr)
			SetShader(static_cast<ShaderStage>(i), info.shaders[i]);
	}

	// Bake shader program
	Bake();
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

void ShaderProgram::Bake()
{
	CombinedResourceLayout resLayout;
	resLayout.descriptorSetMask = 0;

	if (GetShader(ShaderStage::VS))
		resLayout.attributeInputMask = GetShader(ShaderStage::VS)->GetLayout().inputMask;
	if (GetShader(ShaderStage::PS))
		resLayout.renderTargetMask = GetShader(ShaderStage::VS)->GetLayout().outputMask;

	for (U32 i = 0; i < static_cast<U32>(ShaderStage::Count); i++)
	{
		auto shader = GetShader(static_cast<ShaderStage>(i));
		if (shader == nullptr)
			continue;

		U32 stageMask = GetStageMaskFromShaderStage(static_cast<ShaderStage>(i));
		if (stageMask == 0)
			continue;

		const ShaderResourceLayout& shaderResLayout = shader->GetLayout();
		for (U32 set = 0; set < VULKAN_NUM_DESCRIPTOR_SETS; set++)
		{
			bool stageForSet = false;
			for (U32 maskbit = 0; maskbit < DESCRIPTOR_SET_TYPE_COUNT; maskbit++)
			{
				const auto& bindingMask = shaderResLayout.sets[set].masks[maskbit];
				resLayout.sets[set].masks[maskbit] |= bindingMask;

				stageForSet |= bindingMask != 0;

				ForEachBit(bindingMask, [&](U32 bit) {			
					resLayout.stagesForBindings[set][bit] |= stageMask;
					resLayout.sets[set].arraySize[maskbit][bit] = shaderResLayout.sets[set].arraySize[maskbit][bit];
				});
			}
			// Immutable samplers
			{
				const auto& bindingMask = shaderResLayout.sets[set].immutableSamplerMask;
				resLayout.sets[set].immutableSamplerMask = bindingMask;
			
				stageForSet |= bindingMask != 0;

				ForEachBit(bindingMask, [&](U32 bit) {
					resLayout.stagesForBindings[set][bit] |= stageMask;
					resLayout.sets[set].immutableSamplerBindings[bit] = shaderResLayout.sets[set].immutableSamplerBindings[bit];
				});
			}

			if (stageForSet)
				resLayout.stagesForSets[set] |= stageMask;

			if (shaderResLayout.bindlessDescriptorSetMask & (1u << set))
				resLayout.sets[set].isBindless = true;
		}

		// push constants
		if (shaderResLayout.pushConstantSize > 0)
		{
			resLayout.pushConstantRange.stageFlags |= stageMask;
			resLayout.pushConstantRange.size = std::max(resLayout.pushConstantRange.size, shaderResLayout.pushConstantSize);
		}

		resLayout.bindlessSetMask |= shaderResLayout.bindlessDescriptorSetMask;
	}

	// bindings and check array size
	for (U32 set = 0; set < VULKAN_NUM_DESCRIPTOR_SETS; set++)
	{
		if (resLayout.stagesForSets[set] != 0)
		{
			resLayout.descriptorSetMask |= 1u << set;

			for (U32 maskbit = 0; maskbit < DESCRIPTOR_SET_TYPE_COUNT; maskbit++)
			{
				if (resLayout.sets[set].masks[maskbit] == 0)
					continue;

				for (U32 binding = 0; binding < VULKAN_NUM_BINDINGS; binding++)
				{
					U8& arraySize = resLayout.sets[set].arraySize[maskbit][binding];
					if (arraySize == DescriptorSetLayout::UNSIZED_ARRAY)
					{
						// Check is valid bindless
						for (U32 i = 1; i < VULKAN_NUM_BINDINGS; i++)
						{
							if (resLayout.stagesForBindings[set][i] != 0)
								Logger::Error("Invalid bindless for set %u", set);
						}

						// Allows us to have one unified descriptor set layout for bindless.
						resLayout.stagesForBindings[set][binding] = VK_SHADER_STAGE_ALL;
					}
					else if (arraySize == 0)
					{
						// just keep one array size
						arraySize = 1;
					}
				}
			}
		}
	}

	// Create pipeline layout
	HashCombiner hasher;
	hasher.HashCombine(resLayout.pushConstantRange.stageFlags);
	hasher.HashCombine(resLayout.pushConstantRange.size);
	resLayout.pushConstantHash = hasher.Get();
	SetPipelineLayout(device->RequestPipelineLayout(resLayout));
}

void ShaderProgram::SetShader(ShaderStage stage, const Shader* shader)
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

	CreateUpdateTemplates();
}

PipelineLayout::~PipelineLayout()
{
	if (pipelineLayout != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(device.device, pipelineLayout, nullptr);

	for (auto& ut : updateTemplate)
	{
		if (ut != VK_NULL_HANDLE)
			vkDestroyDescriptorUpdateTemplate(device.device, ut, nullptr);
	}
}

void PipelineLayout::CreateUpdateTemplates()
{
	for (unsigned descSet = 0; descSet < VULKAN_NUM_DESCRIPTOR_SETS; descSet++)
	{
		if ((resLayout.descriptorSetMask & (1u << descSet)) == 0)
			continue;
		if ((resLayout.bindlessSetMask & (1u << descSet)) != 0)
			continue;

		VkDescriptorUpdateTemplateEntry updateEntries[VULKAN_NUM_BINDINGS];
		U32 updateCount = 0;
		U32 offset = 0;
		auto& setLayout = resLayout.sets[descSet];
		// Sampled image
		ForEachBit(setLayout.masks[static_cast<U32>(DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE)],
			[&](U32 bit) {
				auto& arraySize = setLayout.arraySize[DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE][bit];
				ASSERT(updateCount < VULKAN_NUM_BINDINGS);
				auto& entry = updateEntries[updateCount++];
				entry.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				entry.dstBinding = GetUnrolledBinding(bit, DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE);
				entry.dstArrayElement = 0;
				entry.descriptorCount = arraySize;
				entry.offset = offsetof(ResourceBinding, image) + sizeof(ResourceBinding) * offset;
				entry.stride = sizeof(ResourceBinding);
				offset+= arraySize;
			});

		// Storage image
		ForEachBit(setLayout.masks[static_cast<U32>(DESCRIPTOR_SET_TYPE_STORAGE_IMAGE)],
			[&](U32 bit) {
				auto& arraySize = setLayout.arraySize[DESCRIPTOR_SET_TYPE_STORAGE_IMAGE][bit];
				ASSERT(updateCount < VULKAN_NUM_BINDINGS);
				auto& entry = updateEntries[updateCount++];
				entry.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				entry.dstBinding = GetUnrolledBinding(bit, DESCRIPTOR_SET_TYPE_STORAGE_IMAGE);
				entry.dstArrayElement = 0;
				entry.descriptorCount = arraySize;
				entry.offset = offsetof(ResourceBinding, image) + sizeof(ResourceBinding) * offset;
				entry.stride = sizeof(ResourceBinding);
				offset += arraySize;
			});

		// Constant buffers
		ForEachBit(setLayout.masks[static_cast<U32>(DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER)],
			[&](U32 bit) {
				auto& arraySize = setLayout.arraySize[DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][bit];
				ASSERT(updateCount < VULKAN_NUM_BINDINGS);
				auto& entry = updateEntries[updateCount++];
				entry.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				entry.dstBinding = GetUnrolledBinding(bit, DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER);
				entry.dstArrayElement = 0;
				entry.descriptorCount = arraySize;
				entry.offset = offsetof(ResourceBinding, buffer) + sizeof(ResourceBinding) * offset;
				entry.stride = sizeof(ResourceBinding);
				offset += arraySize;
			});

		// Input attachment
		ForEachBit(setLayout.masks[static_cast<U32>(DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT)],
			[&](U32 bit) {
				auto& arraySize = setLayout.arraySize[DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT][bit];
				ASSERT(updateCount < VULKAN_NUM_BINDINGS);
				auto& entry = updateEntries[updateCount++];
				entry.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				entry.dstBinding = GetUnrolledBinding(bit, DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT);
				entry.dstArrayElement = 0;
				entry.descriptorCount = arraySize;
				entry.offset = offsetof(ResourceBinding, image) + sizeof(ResourceBinding) * offset;
				entry.stride = sizeof(ResourceBinding);
				offset += arraySize;
			});

		// Sampler
		ForEachBit(setLayout.masks[static_cast<U32>(DESCRIPTOR_SET_TYPE_SAMPLER)],
			[&](U32 bit) {
				auto& arraySize = setLayout.arraySize[DESCRIPTOR_SET_TYPE_SAMPLER][bit];
				ASSERT(updateCount < VULKAN_NUM_BINDINGS);
				auto& entry = updateEntries[updateCount++];
				entry.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				entry.dstBinding = GetUnrolledBinding(bit, DESCRIPTOR_SET_TYPE_SAMPLER);
				entry.dstArrayElement = 0;
				entry.descriptorCount = arraySize;
				entry.offset = offsetof(ResourceBinding, image) + sizeof(ResourceBinding) * offset;
				entry.stride = sizeof(ResourceBinding);
				offset += arraySize;
			});

		VkDescriptorUpdateTemplateCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
		info.pipelineLayout = pipelineLayout;
		info.descriptorSetLayout = descriptorSetAllocators[descSet]->GetSetLayout();
		info.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
		info.set = descSet;
		info.descriptorUpdateEntryCount = updateCount;
		info.pDescriptorUpdateEntries = updateEntries;
		info.pipelineBindPoint = (resLayout.stagesForSets[descSet] & VK_SHADER_STAGE_COMPUTE_BIT) ?
			VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

		if (vkCreateDescriptorUpdateTemplate(device.device, &info, nullptr, &updateTemplate[descSet]) != VK_SUCCESS)
			Logger::Error("Failed to create descriptor update template.");
	}
}

}
}