#pragma once

#include "definition.h"

namespace VulkanTest
{
namespace GPU
{
	enum class BindlessReosurceType
	{
		SampledImage,
		StorageImage,
		StorageBuffer,
		Sampler
	};

	enum DescriptorSetType
	{
		DESCRIPTOR_SET_SAMPLED_IMAGE,
		DESCRIPTOR_SET_STORAGE_IMAGE,
		DESCRIPTOR_SET_UNIFORM_BUFFER,
		DESCRIPTOR_SET_STORAGE_BUFFER,
		DESCRIPTOR_SET_SAMPLED_BUFFER,
		DESCRIPTOR_SET_INPUT_ATTACHMENT,
		DESCRIPTOR_SET_SAMPLER,
		DESCRIPTOR_SET_COUNT,
	};

	struct DescriptorSetLayoutBinding
	{
		U32 unrolledBinding = 0;
		U8 arraySize = 0xff;
	};

	struct DescriptorSetLayout
	{
		U32 masks[DESCRIPTOR_SET_COUNT] = {};
		DescriptorSetLayoutBinding bindings[DESCRIPTOR_SET_COUNT][VULKAN_NUM_BINDINGS];
		bool isBindless = false;
		enum { UNSIZED_ARRAY = 0xff };
	};

	class DescriptorSetAllocator;
	class BindlessDescriptorPool;
	class ImageView;

	struct BindlessDescriptorPoolDeleter
	{
		void operator()(BindlessDescriptorPool* buffer);
	};
	class BindlessDescriptorPool : public Util::IntrusiveHashMapEnabled<BindlessDescriptorPool>, public IntrusivePtrEnabled<BindlessDescriptorPool, BindlessDescriptorPoolDeleter>
	{
	public:
		~BindlessDescriptorPool();

		BindlessDescriptorPool(const BindlessDescriptorPool& rhs) = delete;
		void operator=(const BindlessDescriptorPool& rhs) = delete;

		bool AllocateDescriptors(U32 count);
		void Reset();
		VkDescriptorSet GetDescriptorSet()const;

		void SetTexture(int binding, const ImageView& iamgeView, VkImageLayout imageLayout);

	private:
		friend class DeviceVulkan;
		friend struct BindlessDescriptorPoolDeleter;
		friend class Util::ObjectPool<BindlessDescriptorPool>;

		BindlessDescriptorPool(DeviceVulkan& device_, VkDescriptorPool pool_, DescriptorSetAllocator* allocator_, U32 totalSets_, U32 totalDescriptors_);
		VkDescriptorSet AllocateDescriptorSet(U32 numDescriptors);
		void ResetDescriptorSet(VkDescriptorSet descritprSet);

	private:
		DeviceVulkan& device;
		DescriptorSetAllocator* allocator = nullptr;
		VkDescriptorPool pool = VK_NULL_HANDLE;
		VkDescriptorSet set = VK_NULL_HANDLE;
		U32 totalSets = 0;
		U32 totalDescriptors = 0;
		U32 allocatedSets = 0;
		U32 allocatedDescriptors = 0;
	};
	using BindlessDescriptorPoolPtr = IntrusivePtr<BindlessDescriptorPool>;

	class DescriptorSetAllocator : public Util::IntrusiveHashMapEnabled<DescriptorSetAllocator>
	{
	public:
		DescriptorSetAllocator(DeviceVulkan& device_, const DescriptorSetLayout& layout, const U32* stageForBinds);
		~DescriptorSetAllocator();

		void BeginFrame();
		void Clear();

		VkDescriptorSetLayout GetSetLayout()
		{
			return setLayout;
		}

		std::pair<VkDescriptorSet, bool> GetOrAllocate(U32 threadIndex, HashValue hash);
		VkDescriptorPool AllocateBindlessPool(U32 numSets, U32 numDescriptors);

	private:
		DeviceVulkan& device;
		DescriptorSetLayout layoutInfo;
		VkDescriptorSetLayout setLayout;
		std::vector<VkDescriptorPoolSize> poolSize;

		class DescriptorSetNode : public Util::TempHashMapItem<DescriptorSetNode>
		{
		public:
			explicit DescriptorSetNode(VkDescriptorSet set_) : set(set_) {}
			VkDescriptorSet set;
		};
		
		struct PerThread
		{
			Util::TempHashMap<DescriptorSetNode, 8, true> descriptorSetNodes;
			std::vector<VkDescriptorPool> pools;
			bool shouldBegin = false;
		};
		std::vector<std::unique_ptr<PerThread>> perThreads;

		bool isBindless = false;
	};
}
}