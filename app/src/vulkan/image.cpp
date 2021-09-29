#include "image.h"
#include "vulkan/device.h"

namespace GPU
{

ImageView::ImageView(DeviceVulkan& device_, VkImageView imageView, const ImageViewCreateInfo& info) :
	GraphicsCookie(device.GenerateCookie()),
	device(device_),
	imageView(imageView),
	info(info)
{
}

ImageView::~ImageView()
{
	device.ReleaseImageView(imageView);
}

VkImageView ImageView::GetRenderTargetView(uint32_t layer) const
{
	return imageView;
}

void ImageViewDeleter::operator()(ImageView* imageView)
{
	imageView->device.imageViewPool.free(imageView);
}

void ImageDeleter::operator()(Image* image)
{
	image->device.imagePool.free(image);
}

Image::Image(DeviceVulkan& device_, VkImage image_, VkImageView imageView_, const ImageCreateInfo& info):
	device(device_),
	image(image),
	imageInfo(info)
{
	if (imageView_ != VK_NULL_HANDLE)
	{
		ImageViewCreateInfo imageViewInfo = {};
		imageViewInfo.image = this;
		imageViewInfo.format = info.format;
		imageView = ImageViewPtr(device.imageViewPool.allocate(device_, imageView_, imageViewInfo));
	}
}

Image::~Image()
{
	// 如果是自身持有image，需要在析构时释放VkImage
	if (isOwnsImge)
	{
		device.ReleaseImage(image);
	}
}

}