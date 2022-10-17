#pragma once

#include "content\config.h"
#include "core\utils\path.h"
#include "core\utils\dataChunk.h"
#include "core\types\guid.h"

namespace VulkanTest
{
#define INVALID_CHUNK_INDEX (-1)
#define GET_CHUNK_FLAG(chunkIndex) (1 << chunkIndex)

	class ResourceManager;

	// Resource type
	struct VULKAN_TEST_API ResourceType
	{
		static const ResourceType INVALID_TYPE;
		StringID type;

	public:
		ResourceType() = default;
		ResourceType(U64 hash) : type(StringID::FromU64(hash)) {}
		explicit ResourceType(const char* typeName);
		bool operator !=(const ResourceType& rhs) const {
			return rhs.type != type;
		}
		bool operator ==(const ResourceType& rhs) const {
			return rhs.type == type;
		}
		bool operator <(const ResourceType& rhs) const {
			return rhs.type.GetHashValue() < type.GetHashValue();
		}
		U64 GetHashValue()const {
			return type.GetHashValue();
		}

#ifdef CJING3D_EDITOR
	public:
		static HashMap<U64, String> ResourceNameMapping;
		static void RegisterResourceTypename(const ResourceType& type, const String& name);
		static String GetResourceTypename(const ResourceType& type);
#endif
	};

	struct ResourceHeader
	{
		Guid guid;
		ResourceType type;
		DataChunk* chunks[MAX_RESOURCE_DATA_CHUNKS];

		ResourceHeader()
		{
			memset(chunks, 0, sizeof(chunks));
		}
	};

	struct ResourceInitData
	{
		ResourceHeader header;
		OutputMemoryStream customData;

#ifdef CJING3D_EDITOR
		OutputMemoryStream metadata;
		Array<Pair<Guid, U64>> dependencies;
#endif
	};
}