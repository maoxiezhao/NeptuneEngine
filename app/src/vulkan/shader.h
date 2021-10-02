#pragma once

#include "definition.h"
#include "descriptorSet.h"

namespace GPU
{
	struct ShaderResourceLayout
	{
		DescriptorSetLayout sets[VULKAN_NUM_DESCRIPTOR_SETS];
		uint32_t pushConstantSize = 0;
	};

	struct CombinedResourceLayout
	{
		U32 descriptorSetMask = 0;
		U32 stagesForSets[VULKAN_NUM_DESCRIPTOR_SETS] = {};
		U32 stagesForBindings[VULKAN_NUM_DESCRIPTOR_SETS][VULKAN_NUM_BINDINGS] = {};
		DescriptorSetLayout sets[VULKAN_NUM_DESCRIPTOR_SETS];
		VkPushConstantRange pushConstantRange = {};
		HashValue pushConstantHash = 0;
	};

	class PipelineLayout : public HashedObject<PipelineLayout>
	{
	public:
		PipelineLayout(DeviceVulkan& device_, CombinedResourceLayout resourceLayout);
		~PipelineLayout();

		VkPipelineLayout GetLayout()const
		{
			return pipelineLayout;
		}

	private:
		DeviceVulkan& device;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorLayouts[VULKAN_NUM_DESCRIPTOR_SETS] = {};
		DescriptorSetAllocator* descriptorSetAllocators[VULKAN_NUM_DESCRIPTOR_SETS] = {};
	};

	class Shader : public HashedObject<Shader>
	{
	public:
		Shader(DeviceVulkan& device_, ShaderStage shaderStage_, VkShaderModule shaderModule_);
		Shader(DeviceVulkan& device_, ShaderStage shaderStage_, const void* pShaderBytecode, size_t bytecodeLength);
		~Shader();

		VkShaderModule GetModule()const
		{
			return shaderModule;
		}

		const ShaderResourceLayout& GetLayout()const
		{
			return layout;
		}

	private:
		DeviceVulkan& device;
		ShaderStage shaderStage;
		VkShaderModule shaderModule = VK_NULL_HANDLE;
		ShaderResourceLayout layout;
	};

	struct ShaderProgramInfo
	{
		Shader* shaders[static_cast<U32>(ShaderStage::Count)] = {};
	};

	struct ShaderProgram : public HashedObject<Shader>
	{
	public:
		ShaderProgram(DeviceVulkan* device_, const ShaderProgramInfo& info);
		~ShaderProgram();

		PipelineLayout* GetPipelineLayout()const
		{
			return pipelineLayout;
		}

		inline const Shader* GetShader(ShaderStage stage) const
		{
			return shaders[UINT(stage)];
		}

		bool IsEmpty()const
		{
			return shaderCount > 0;
		}

		void SetPipelineLayout(PipelineLayout* layout)
		{
			pipelineLayout = layout;
		}

		void AddPipeline(HashValue hash, VkPipeline pipeline);
		VkPipeline GetPipeline(HashValue hash);

	private:
		void SetShader(ShaderStage stage, Shader* shader);

		DeviceVulkan* device;
		PipelineLayout* pipelineLayout = nullptr;
		Shader* shaders[UINT(ShaderStage::Count)] = {};
		uint32_t shaderCount = 0;
		std::unordered_map<HashValue, VkPipeline> pipelines;
	};

}