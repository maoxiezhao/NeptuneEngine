#pragma once

#include "core\common.h"
#include "resourceHeader.h"

namespace VulkanTest
{
	struct ResourceInfo
	{
		Guid guid;
		ResourceType type;
		Path path;

	public:
		ResourceInfo() :
			guid(Guid::Empty),
			type(ResourceType::INVALID_TYPE)
		{
		}

		ResourceInfo(const Guid& guid_, const ResourceType& type_, const Path& path_) :
			guid(guid_),
			type(type_),
			path(path_)
		{
		}

		static ResourceInfo Temporary(const ResourceType& type_)
		{
			return ResourceInfo(
				Guid::New(),
				type_,
				Path("Temporary")
			);
		}
	};
}