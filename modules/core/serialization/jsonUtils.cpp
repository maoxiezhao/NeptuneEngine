#include "jsonUtils.h"

namespace VulkanTest
{
	Guid JsonUtils::GetGuid(const Value& value)
	{
		if (value.IsNull())
			return Guid::Empty;
		if (value.GetStringLength() != 32)
			return Guid::Empty;

		const char* a = value.GetString();
		const char* b = a + 8;
		const char* c = b + 8;
		const char* d = c + 8;

		Guid ret;
		FromHexString(Span(a, 8), ret.A);
		FromHexString(Span(b, 8), ret.B);
		FromHexString(Span(c, 8), ret.C);
		FromHexString(Span(d, 8), ret.D);
		return ret;
	}

	ECS::Entity JsonUtils::GetEntity(World* world, const Value& value)
	{
		const char* path = value.GetString();
		if (!path || StringLength(path) <= 0)
			return ECS::INVALID_ENTITY;

		return world->Entity(path);
	}
}