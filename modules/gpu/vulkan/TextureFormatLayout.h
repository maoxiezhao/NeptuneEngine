#pragma once

#include "definition.h"

namespace GPU
{
	class DeviceVulkan;
	
	class TextureFormatLayout
	{
	public:
		void SetTexture1D(VkFormat format, U32 width, U32 height, U32 arrayLayers = 1, U32 mipLevels = 1);
		void SetTexture2D(VkFormat format, U32 width, U32 height, U32 arrayLayers = 1, U32 mipLevels = 1);
		void SetTexture3D(VkFormat format, U32 width, U32 height, U32 arrayLayers = 1, U32 mipLevels = 1);

		U32 GetRequiredSize()const;
		void BuildBufferImageCopies();
	};
}