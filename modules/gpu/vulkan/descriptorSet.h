#pragma once

#include "definition.h"
#include "buffer.h"
#include "core\utils\threadLocal.h"

namespace VulkanTest
{
namespace GPU
{
	enum class BindlessReosurceType
	{
		SampledImage,
		StorageImage,
		StorageBuffer,
		Sampler,
		Count
	};

	struct DescriptorSetLayout
	{
		U32 masks[DESCRIPTOR_SET_TYPE_COUNT] = {};
		U8 arraySize[DESCRIPTOR_SET_TYPE_COUNT][VULKAN_NUM_BINDINGS];
		U8 resourceType[DESCRIPTOR_SET_TYPE_COUNT][VULKAN_NUM_BINDINGS];
		bool isBindless = false;
		U32 immutableSamplerMask = 0;
		U32 immutableSamplerBindings[VULKAN_NUM_BINDINGS];
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
		void SetBuffer(int binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
		void SetUniformTexelBuffer(int binding, VkBufferView bufferView);

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

		I32 GetDescriptorCount()const {
			return descriptorCount;
		}

		VkDescriptorSetLayout GetSetLayout() {
			return setLayout;
		}

		std::pair<VkDescriptorSet, bool> GetOrAllocate(HashValue hash);
		VkDescriptorPool AllocateBindlessPool(U32 numSets, U32 numDescriptors);

	private:
		DeviceVulkan& device;
		DescriptorSetLayout layoutInfo;
		VkDescriptorSetLayout setLayout;
		std::vector<VkDescriptorPoolSize> poolSize;
		I32 descriptorCount = 0;

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
		ThreadLocalObject<PerThread> perThreads;

		bool isBindless = false;
	};

	class BindlessDescriptorHandler;
	class BindlessDescriptorHeap;

	struct BindlessDescriptorHandlerDeleter
	{
		void operator()(BindlessDescriptorHandler* buffer);
	};
	class VULKAN_TEST_API BindlessDescriptorHandler : public IntrusivePtrEnabled<BindlessDescriptorHandler, BindlessDescriptorHandlerDeleter>, public InternalSyncObject
	{
	public:
		~BindlessDescriptorHandler();

		BindlessDescriptorHandler(const BindlessDescriptorHandler& rhs) = delete;
		void operator=(const BindlessDescriptorHandler& rhs) = delete;
	
		I32 GetIndex()const {
			return index;
		}

	private:
		friend class DeviceVulkan;
		friend struct BindlessDescriptorHandlerDeleter;
		friend class Util::ObjectPool<BindlessDescriptorHandler>;

		BindlessDescriptorHandler(DeviceVulkan& device_, BindlessReosurceType type_, I32 index_);

		DeviceVulkan& device;
		BindlessReosurceType type;
		I32 index;
	};
	using BindlessDescriptorPtr = IntrusivePtr<BindlessDescriptorHandler>;

	class VULKAN_TEST_API BindlessDescriptorHeap
	{
	public:
		BindlessDescriptorHeap() = default;
		~BindlessDescriptorHeap() = default;

		void Init(DeviceVulkan& device, DescriptorSetAllocator* setAllocator);
		void Destroy();
		I32 Allocate();
		void Free(I32 index);

		VkDescriptorSet GetDescriptorSet()const;
		BindlessDescriptorPool& GetPool();

		bool IsInitialized()const {
			return initialized;
		}

	private:
		BindlessDescriptorPoolPtr pool;
		Array<I32> freelist;
		bool initialized = false;
	};
}
}