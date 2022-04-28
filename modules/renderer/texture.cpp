#include "texture.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Texture);

	Texture::Texture(const Path& path_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_)
	{
	}

	Texture::~Texture()
	{
	}

	bool Texture::OnLoaded(U64 size, const U8* mem)
	{
		return false;
	}

	void Texture::OnUnLoaded()
	{
	}
}