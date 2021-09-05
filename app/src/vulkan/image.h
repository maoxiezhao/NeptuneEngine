#pragma once

#include "definition.h"

class DeviceVulkan;
class ImageView;
class Image;

struct ImageViewDeleter
{
    void operator()(ImageView* imageView);
};
class ImageView : public Util::IntrusivePtrEnabled<ImageView, ImageViewDeleter>
{
public:
    ImageView(DeviceVulkan& device, VkImageView imageView, const ImageViewCreateInfo& info);
    ~ImageView();

    VkImageView GetImageView()
    {
        return mImageView;
    }

private:
    friend class DeviceVulkan;
    friend struct ImageViewDeleter;
    friend class Util::ObjectPool<ImageView>;
    
    DeviceVulkan& mDevice;
    VkImageView mImageView;
    ImageViewCreateInfo mInfo;

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
        return mImage;
    }

    ImageViewPtr GetImageViewPtr()
    {
        return mImageView;
    }

    ImageView& GetImageView()
    {
        return *mImageView;
    }

    const ImageView& GetImageView()const
    {
        return *mImageView;
    }

    void DisownImge()
    {
        mIsOwnsImge = false;
    }

private:
    friend class DeviceVulkan;
    friend struct ImageDeleter;
    friend class Util::ObjectPool<Image>;

    Image(DeviceVulkan& device, VkImage image, VkImageView imageView, const ImageCreateInfo& info);

private:
    DeviceVulkan& mDevice;
    VkImage mImage;
    ImageViewPtr mImageView;
    ImageCreateInfo mImageInfo;
    bool mIsOwnsImge = true;
};
using ImagePtr = Util::IntrusivePtr<Image>;