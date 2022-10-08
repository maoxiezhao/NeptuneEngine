#pragma once

#include "core\common.h"

#define SHADER_RESOURCE_CHUNK_MATERIAL_PARAMS 0
#define SHADER_RESOURCE_CHUNK_SHADER_CACHE 1
#define SHADER_RESOURCE_CHUNK_SOURCE 15

namespace VulkanTest
{
	class ShaderStorage
	{
	public:
		enum CachingMode
		{
			Disabled, 
			ResourceInternal, 
			ProjectCache
		};
		static CachingMode CurrentCachingMode;

#pragma pack(1)
		struct Header
		{
			static const U32 FILE_MAGIC;
			static const U32 FILE_VERSION;

			U32 magic;
			U32 version;
		};
#pragma pack()
	};
}