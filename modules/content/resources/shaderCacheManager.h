#pragma once

#include "core\types\guid.h"
#include "shaderStorage.h"
#include "gpu\vulkan\device.h"
#include "gpu\vulkan\shader.h"

namespace VulkanTest
{
	class ShaderCacheManager
	{
	public:
        struct CachedEntry
        {
            Guid ID = Guid::Empty;
            Path Path;

            bool IsValid() const;
            bool Exists() const;
            U64 GetLastModTime() const;
        };

		static bool TryGetEntry(const Guid& id, CachedEntry& cachedEntry);
        static bool GetCache(const CachedEntry& cachedEntry, OutputMemoryStream& output);
        static bool SaveCache(const CachedEntry& cachedEntry, OutputMemoryStream& output);
	};
}