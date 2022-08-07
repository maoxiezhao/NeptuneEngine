#pragma once

#include "definition.h"

namespace VulkanTest
{
namespace GPU
{
	static inline const char* LayoutToString(VkImageLayout layout)
	{
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return "SHADER_READ_ONLY";
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return "DS_READ_ONLY";
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return "DS";
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return "COLOR";
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return "TRANSFER_DST";
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return "TRANSFER_SRC";
		case VK_IMAGE_LAYOUT_GENERAL:
			return "GENERAL";
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			return "PRESENT";
		default:
			return "UNDEFINED";
		}
	}

	static inline std::string AccessFlagsToString(VkAccessFlags flags)
	{
		std::string result;
		if (flags & VK_ACCESS_SHADER_READ_BIT)
			result += "SHADER_READ ";
		if (flags & VK_ACCESS_SHADER_WRITE_BIT)
			result += "SHADER_WRITE ";
		if (flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
			result += "DS_WRITE ";
		if (flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
			result += "DS_READ ";
		if (flags & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
			result += "COLOR_READ ";
		if (flags & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
			result += "COLOR_WRITE ";
		if (flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
			result += "INPUT_READ ";
		if (flags & VK_ACCESS_TRANSFER_WRITE_BIT)
			result += "TRANSFER_WRITE ";
		if (flags & VK_ACCESS_TRANSFER_READ_BIT)
			result += "TRANSFER_READ ";
		if (flags & VK_ACCESS_UNIFORM_READ_BIT)
			result += "UNIFORM_READ ";

		if (!result.empty())
			result.pop_back();
		else
			result = "NONE";

		return result;
	}

	static inline std::string StageFlagsToString(VkPipelineStageFlags flags)
	{
		std::string result;

		if (flags & VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT)
			result += "GRAPHICS ";
		if (flags & (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT))
			result += "DEPTH ";
		if (flags & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
			result += "COLOR ";
		if (flags & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
			result += "FRAGMENT ";
		if (flags & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
			result += "COMPUTE ";
		if (flags & VK_PIPELINE_STAGE_TRANSFER_BIT)
			result += "TRANSFER ";
		if (flags & (VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT))
			result += "VERTEX ";

		if (!result.empty())
			result.pop_back();
		else
			result = "NONE";

		return result;
	}

	static inline const char* FormatToString(VkFormat format)
	{
		switch (format)
		{
		case VK_FORMAT_R8_UNORM:
			return "R8_UNORM";
		case VK_FORMAT_R8_SNORM:
			return "R8_SNORM";
		case VK_FORMAT_R8_UINT:
			return "R8_UINT";
		case VK_FORMAT_R8_SINT:
			return "R8_SINT";
		case VK_FORMAT_R8_SRGB:
			return "R8_SRGB";
		case VK_FORMAT_R8G8_UNORM:
			return "R8_UINT";
		case VK_FORMAT_R8G8_SNORM:
			return "R8G8_SNORM";
		case VK_FORMAT_R8G8_UINT:
			return "R8G8_UINT";
		case VK_FORMAT_R8G8_SINT:
			return "R8G8_SINT";
		case VK_FORMAT_R8G8_SRGB:
			return "R8G8_SRGB";
		case VK_FORMAT_R8G8B8_UNORM:
			return "R8G8B8_UNORM";
		case VK_FORMAT_R8G8B8_SNORM:
			return "R8G8B8_SNORM";
		case VK_FORMAT_R8G8B8_UINT:
			return "R8G8B8_UINT";
		case VK_FORMAT_R8G8B8_SINT:
			return "R8G8B8_SINT";
		case VK_FORMAT_R8G8B8_SRGB:
			return "R8G8B8_SRGB";
		case VK_FORMAT_R8G8B8A8_UNORM:
			return "R8G8B8A8_UNORM";
		case VK_FORMAT_R8G8B8A8_SNORM:
			return "R8G8B8A8_SNORM";
		case VK_FORMAT_R8G8B8A8_UINT:
			return "R8G8B8A8_UINT";
		case VK_FORMAT_R8G8B8A8_SINT:
			return "R8G8B8A8_SINT";
		case VK_FORMAT_R8G8B8A8_SRGB:
			return "R8G8B8A8_SRGB";
		case VK_FORMAT_R16_UNORM:
			return "R16_UNORM";
		case VK_FORMAT_R16_SNORM:
			return "R16_SNORM";
		case VK_FORMAT_R16_UINT:
			return "R16_UINT";
		case VK_FORMAT_R16_SINT:
			return "R16_SINT";
		case VK_FORMAT_R16_SFLOAT:
			return "R16_SFLOAT";
		case VK_FORMAT_R32_UINT:
			return "R32_UINT";
		case VK_FORMAT_R32_SINT:
			return "R32_SINT";
		case VK_FORMAT_R32_SFLOAT:
			return "R32_SFLOAT";
		case VK_FORMAT_R32G32_UINT:
			return "RG32_UINT";
		case VK_FORMAT_R32G32_SINT:
			return "RG32_SINT";
		case VK_FORMAT_R32G32_SFLOAT:
			return "RG32_SFLOAT";
		case VK_FORMAT_R32G32B32_UINT:
			return "RGB32_UINT";
		case VK_FORMAT_R32G32B32_SINT:
			return "RGB32_SINT";
		case VK_FORMAT_R32G32B32_SFLOAT:
			return "RGB32_SFLOAT";
		case VK_FORMAT_R32G32B32A32_UINT:
			return "RGBA32_UINT";
		case VK_FORMAT_R32G32B32A32_SINT:
			return "RGBA32_SINT";
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return "RGBA32_SFLOAT";
		default:
			return "UNKNOWN";
		}
	}
}
}