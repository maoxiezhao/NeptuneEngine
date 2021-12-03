#include "definition.h"
#include "device.h"

namespace VulkanTest
{
namespace GPU
{
	void SamplerDeleter::operator()(Sampler* sampler)
	{
		sampler->device.samplers.free(sampler);
	}

	Sampler::~Sampler()
	{
		if (sampler != VK_NULL_HANDLE)
		{
			if (isImmutable)
				vkDestroySampler(device.device, sampler, nullptr);
			else
				device.ReleaseSampler(sampler);
		}
	}

	Sampler::Sampler(DeviceVulkan& device_, VkSampler sampler_, const SamplerCreateInfo& createInfo_, bool isImmutable_) :
		GraphicsCookie(device_.GenerateCookie()),
		device(device_),
		sampler(sampler_),
		createInfo(createInfo_),
		isImmutable(isImmutable_)
	{
	}

	ImmutableSampler::ImmutableSampler(DeviceVulkan& device_, const SamplerCreateInfo& createInfo_) :
		device(device_)
	{
		SamplerPtr ptr = device.RequestSampler(createInfo_, true);
		if (ptr)
			sampler = ptr;
	}
}
}