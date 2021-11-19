#include "definition.h"
#include "device.h"

namespace GPU
{
	void SamplerDeleter::operator()(Sampler* sampler)
	{
		sampler->device.samplers.free(sampler);
	}

	Sampler::~Sampler()
	{
		if (sampler != VK_NULL_HANDLE)
			device.ReleaseSampler(sampler);
	}

	Sampler::Sampler(DeviceVulkan& device_, VkSampler sampler_, const SamplerCreateInfo& createInfo_) :
		GraphicsCookie(device_.GenerateCookie()),
		device(device_),
		sampler(sampler_),
		createInfo(createInfo_)
	{
	}

	ImmutableSampler::ImmutableSampler(DeviceVulkan& device_, const SamplerCreateInfo& createInfo_) :
		device(device_)
	{
		SamplerPtr ptr = device.RequestSampler(createInfo_);
		if (ptr)
			sampler = ptr;
	}
}