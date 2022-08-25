#include "fontResource.h"
#include "fontInclude.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>

namespace VulkanTest
{
	DEFINE_RESOURCE(FontResource);

	FontResource::FontResource(const Path& path_, ResourceFactory& resFactory_) :
		BinaryResource(path_, resFactory_)
	{
	}

	FontResource::~FontResource()
	{
		ASSERT(IsEmpty());
	}


	bool FontResource::Init(ResourceInitData& initData)
	{
		InputMemoryStream inputMem(initData.customData);
		inputMem.Read<FontHeader>(header);
		if (header.magic != FontHeader::MAGIC)
		{
			Logger::Warning("Unsupported font file %s", GetPath());
			return false;
		}

		if (header.version != FontHeader::LAST_VERSION)
		{
			Logger::Warning("Unsupported version of font %s", GetPath());
			return false;
		}
		return true;
	}

	bool FontResource::Load()
	{
		PROFILE_FUNCTION();
		const auto dataChunk = GetChunk(0);
		if (dataChunk == nullptr || !dataChunk->IsLoaded())
			return false;


		return true;
	}

	void FontResource::Unload()
	{
	}


	FontResourceFactory::FontResourceFactory()
	{
	}

	FontResourceFactory::~FontResourceFactory()
	{
	}

	Resource* FontResourceFactory::CreateResource(const Path& path)
	{
		return CJING_NEW(FontResource)(path, *this);
	}
}