#pragma once

#include "core\common.h"
#include "gpu\vulkan\device.h"

namespace VulkanTest::ImageUtil
{
    struct VULKAN_TEST_API Params
    {

    };

    void Draw(GPU::Image* image, Params params, GPU::CommandList& cmd);
}