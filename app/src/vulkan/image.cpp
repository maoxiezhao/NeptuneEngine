#include "image.h"
#include "vulkan/device.h"

ImageView::ImageView(DeviceVulkan& device, VkImageView imageView, const ImageViewCreateInfo& info) :
	GraphicsCookie(device.GenerateCookie()),
	mDevice(device),
	mImageView(imageView),
	mInfo(info)
{
}

ImageView::~ImageView()
{
	mDevice.ReleaseImageView(mImageView);
}

VkImageView ImageView::GetRenderTargetView(uint32_t layer) const
{
	return mImageView;
}

void ImageViewDeleter::operator()(ImageView* imageView)
{
	imageView->mDevice.mImageViewPool.free(imageView);
}

void ImageDeleter::operator()(Image* image)
{
	image->mDevice.mImagePool.free(image);
}

Image::Image(DeviceVulkan& device, VkImage image, VkImageView imageView, const ImageCreateInfo& info):
	mDevice(device),
	mImage(image),
	mImageInfo(info)
{
	if (imageView != VK_NULL_HANDLE)
	{
		ImageViewCreateInfo imageViewInfo = {};
		imageViewInfo.mImage = this;
		imageViewInfo.mFormat = info.mFormat;
		mImageView = ImageViewPtr(device.mImageViewPool.allocate(device, imageView, imageViewInfo));
	}
}

Image::~Image()
{
	// 如果是自身持有image，需要在析构时释放VkImage
	if (mIsOwnsImge)
	{
		mDevice.ReleaseImage(mImage);
	}
}