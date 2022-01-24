#include "image.h"
#include "vulkan/device.h"

namespace VulkanTest
{
namespace GPU
{

ImageView::ImageView(DeviceVulkan& device_, VkImageView imageView, const ImageViewCreateInfo& info) :
	GraphicsCookie(device_),
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


VkPipelineStageFlags Image::ConvertUsageToPossibleStages(VkImageUsageFlags usage)
{
	VkPipelineStageFlags flags = 0;

	if (usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

	if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
	{
		flags |= 
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | 
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
		flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	if (usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
		flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
	{
		VkPipelineStageFlags possible = 
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

		if (usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
			possible |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

		flags &= possible;
	}
	return flags;
}

VkAccessFlags Image::ConvertUsageToPossibleAccess(VkImageUsageFlags usage)
{
	VkAccessFlags flags = 0;
	if (usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		flags |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	if (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
		flags |= VK_ACCESS_SHADER_READ_BIT;
	if (usage & VK_IMAGE_USAGE_STORAGE_BIT)
		flags |= VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	if (usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
		flags |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

	// Transient attachments can only be attachments, and never other resources.
	if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
	{
		flags &= 
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | 
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | 
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	}

	return flags;
}

VkAccessFlags Image::ConvertLayoutToPossibleAccess(VkImageLayout layout)
{
	switch (layout)
	{
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		return VK_ACCESS_TRANSFER_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return VK_ACCESS_TRANSFER_WRITE_BIT;
	default:
		return ~0u;
	}
}

Image::Image(DeviceVulkan& device_, VkImage image_, VkImageView imageView_, const DeviceAllocation& allocation_, const ImageCreateInfo& info):
	GraphicsCookie(device_),
	device(device_),
	image(image_),
	allocation(allocation_),
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
	if (isOwnsImge)
		device.ReleaseImage(image);

	if (isOwnsMemory)
		device.FreeMemory(allocation);
}

}
}