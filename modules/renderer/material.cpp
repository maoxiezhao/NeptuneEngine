#include "material.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Material);

	Material::Material(const Path& path_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_)
	{
	}

	Material::~Material()
	{
	}

	bool Material::OnLoaded(U64 size, const U8* mem)
	{
		return false;
	}

	void Material::OnUnLoaded()
	{
	}
}