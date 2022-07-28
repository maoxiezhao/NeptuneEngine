#pragma once

#include "definition.h"
#include "descriptorSet.h"
#include "sampler.h"

namespace VulkanTest
{
namespace GPU
{
	struct ShaderMacro
	{
		std::string name;
		int definition = 0;
	};

	struct ShaderResourceLayout
	{
		DescriptorSetLayout sets[VULKAN_NUM_DESCRIPTOR_SETS];
		U32 pushConstantSize = 0;
		U32 inputMask = 0;
		U32 outputMask = 0;
		U32 bindlessDescriptorSetMask = 0;
	};

	struct CombinedResourceLayout
	{
		U32 attributeInputMask = 0;
		U32 renderTargetMask = 0;
		U32 descriptorSetMask = 0;
		U32 stagesForSets[VULKAN_NUM_DESCRIPTOR_SETS] = {};
		U32 stagesForBindings[VULKAN_NUM_DESCRIPTOR_SETS][VULKAN_NUM_BINDINGS] = {};
		DescriptorSetLayout sets[VULKAN_NUM_DESCRIPTOR_SETS];
		VkPushConstantRange pushConstantRange = {};
		HashValue pushConstantHash = 0;
		U32 bindlessSetMask = 0;
	};

	struct ResourceBinding
	{
		union
		{
			VkDescriptorBufferInfo buffer;
			VkDescriptorImageInfo image;
		};
		VkDeviceSize dynamicOffset; // Constant buffer dynamic offset
	};

	struct ResourceBindings
	{
		ResourceBinding bindings[VULKAN_NUM_DESCRIPTOR_SETS][DESCRIPTOR_SET_TYPE_COUNT][VULKAN_NUM_BINDINGS];
		uint64_t cookies[VULKAN_NUM_DESCRIPTOR_SETS][DESCRIPTOR_SET_TYPE_COUNT][VULKAN_NUM_BINDINGS];
		uint8_t pushConstantData[VULKAN_PUSH_CONSTANT_SIZE];
	};

	class PipelineLayout : public Util::IntrusiveHashMapEnabled<PipelineLayout>
	{
	public:
		PipelineLayout(DeviceVulkan& device_, CombinedResourceLayout resLayout_);
		~PipelineLayout();

		VkPipelineLayout GetLayout()const
		{
			return pipelineLayout;
		}

		const CombinedResourceLayout& GetResLayout()const
		{
			return resLayout;
		}

		DescriptorSetAllocator* GetAllocator(U32 set)const
		{
			return descriptorSetAllocators[set];
		}

		VkDescriptorUpdateTemplate GetUpdateTemplate(U32 set)const
		{
			return updateTemplate[set];
		}

	private:
		void CreateUpdateTemplates();

		DeviceVulkan& device;
		CombinedResourceLayout resLayout;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		DescriptorSetAllocator* descriptorSetAllocators[VULKAN_NUM_DESCRIPTOR_SETS] = {};
		VkDescriptorUpdateTemplate updateTemplate[VULKAN_NUM_DESCRIPTOR_SETS] = {};
	};

	class Shader : public Util::IntrusiveHashMapEnabled<Shader>
	{
	public:
		Shader(DeviceVulkan& device_, ShaderStage shaderStage_, const void* pShaderBytecode, size_t bytecodeLength, const ShaderResourceLayout* layout_ = nullptr);
		~Shader();

		VkShaderModule GetModule()const
		{
			return shaderModule;
		}

		ShaderStage GetStage()const 
		{
			return shaderStage;
		}

		const ShaderResourceLayout& GetLayout()const
		{
			return layout;
		}

		void SetEntryPoint(const char* entryPoint_);
		const char* GetEntryPoint()const;

		static bool ReflectShader(ShaderResourceLayout& layout, const U32* spirvData, size_t spirvSize);

	private:
		DeviceVulkan& device;
		ShaderStage shaderStage;
		VkShaderModule shaderModule = VK_NULL_HANDLE;
		ShaderResourceLayout layout;
		std::string entryPoint;
	};

	struct ShaderProgramInfo
	{
		const Shader* shaders[static_cast<U32>(ShaderStage::Count)] = {};
	};

	struct ShaderProgram;
	struct ShaderProgramDeleter
	{
		void operator()(ShaderProgram* owner);
	};
	struct ShaderProgram : public Util::IntrusiveHashMapEnabled<ShaderProgram>, public InternalSyncObject, public IntrusivePtrEnabled<ShaderProgram, ShaderProgramDeleter>
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
		void MoveToReadOnly();

	private:
		friend struct ShaderProgramDeleter;
		friend class Util::ObjectPool<ShaderProgram>;

		void Bake();
		void SetShader(ShaderStage stage, const Shader* shader);

		DeviceVulkan* device;
		PipelineLayout* pipelineLayout = nullptr;
		const Shader* shaders[UINT(ShaderStage::Count)] = {};
		U32 shaderCount = 0;
		VulkanCache<Util::IntrusivePODWrapper<VkPipeline>> pipelines;
	};
	using ShaderProgramPtr = IntrusivePtr<ShaderProgram>;
}
}