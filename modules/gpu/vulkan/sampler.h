#pragma once

#include "definition.h"
#include "cookie.h"

namespace VulkanTest
{
namespace GPU
{
	class DeviceVulkan;

    enum class StockSampler
    {
        NearestClamp,
        PointClamp,
        Count
    };

    class Sampler;
    struct SamplerDeleter
    {
        void operator()(Sampler* sampler);
    };
    class Sampler : public IntrusivePtrEnabled<Sampler, SamplerDeleter>, public GraphicsCookie, public InternalSyncObject
    {
    public:
        ~Sampler();

        VkSampler GetSampler()const
        {
            return sampler;
        }

        SamplerCreateInfo GetCreateInfo()const
        {
            return createInfo;
        }

    private:
        friend class DeviceVulkan;
        friend struct SamplerDeleter;
        friend class Util::ObjectPool<Sampler>;

        Sampler(DeviceVulkan& device_, VkSampler sampler_, const SamplerCreateInfo& createInfo_, bool isImmutable_);

    private:
        DeviceVulkan& device;
        VkSampler sampler;
        SamplerCreateInfo createInfo;
        bool isImmutable;
    };
    using SamplerPtr = IntrusivePtr<Sampler>;

    class ImmutableSampler : public HashedObject<ImmutableSampler>
    {
    public:
        ImmutableSampler(DeviceVulkan& device_, const SamplerCreateInfo& createInfo_);
        ImmutableSampler(const ImmutableSampler& rhs) = delete;
        void operator=(const ImmutableSampler& rhs) = delete;

        Sampler& GetSampler()
        {
            return *sampler;
        }
        const Sampler& GetSampler()const
        {
            return *sampler;
        }

        SamplerPtr GetSamplerPtr()
        {
            return sampler;
        }
        const SamplerPtr GetSamplerPtr()const
        {
            return sampler;
        }

        explicit operator bool() const
        {
            return sampler.operator bool();
        }

    private:
        DeviceVulkan& device;
        SamplerPtr sampler;
    };
}
}