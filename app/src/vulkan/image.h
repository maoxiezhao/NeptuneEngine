#pragma once

#include "definition.h"

namespace GPU
{

class DeviceVulkan;
class ImageView;
class Image;

struct ImageViewDeleter
{
    void operator()(ImageView* imageView);
};
class ImageView : public Util::IntrusivePtrEnabled<ImageView, ImageViewDeleter>, public GraphicsCookie
{
public:
    ImageView(DeviceVulkan& device_, VkImageView imageView_, const ImageViewCreateInfo& info_);
    ~ImageView();

    VkImageView GetImageView()
    {
        return imageView;
    }

	Image* GetImage()
	{
		return info.image;
	}

    const Image* GetImage() const
    {
        return info.image;
    }

    VkFormat GetFormat()const
    {
        return info.format;
    }

    const ImageViewCreateInfo& GetInfo()const
    {
        return info;
    }

    VkImageView GetRenderTargetView(uint32_t layer)const;

private:
    friend class DeviceVulkan;
    friend struct ImageViewDeleter;
    friend class Util::ObjectPool<ImageView>;
    
    DeviceVulkan& device;
    VkImageView imageView;
    ImageViewCreateInfo info;
};
using ImageViewPtr = Util::IntrusivePtr<ImageView>;

struct ImageDeleter
{
    void operator()(Image* image);
};
class Image : public Util::IntrusivePtrEnabled<Image, ImageDeleter>
{
public:
    ~Image();

    VkImage GetImage()
    {
        return image;
    }

    ImageViewPtr GetImageViewPtr()
    {
        return imageView;
    }

    ImageView& GetImageView()
    {
        return *imageView;
    }

    const ImageView& GetImageView()const
    {
        return *imageView;
    }

    void DisownImge()
    {
        isOwnsImge = false;
    }

    VkImageLayout GetSwapchainLayout() const
    {
        return swapchainLayout;
    }

    void SetSwapchainLayout(VkImageLayout layout)
    {
        swapchainLayout = layout;
    }

    bool IsSwapchainImage()const
    {
        return swapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED;
    }

    uint32_t GetWidth() const
    {
        return imageInfo.width;
    }

    uint32_t GetHeight() const
    {
        return imageInfo.height;
    }

private:
    friend class DeviceVulkan;
    friend struct ImageDeleter;
    friend class Util::ObjectPool<Image>;

    Image(DeviceVulkan& device_, VkImage image_, VkImageView imageView_, const ImageCreateInfo& info_);

private:
    DeviceVulkan& device;
    VkImage image;
    ImageViewPtr imageView;
    ImageCreateInfo imageInfo;
    VkImageLayout swapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool isOwnsImge = true;
};
using ImagePtr = Util::IntrusivePtr<Image>;

}