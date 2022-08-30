#include "fontResource.h"
#include "fontManager.h"
#include "font.h"
#include "core\engine.h"

#include "ft2build.h"
#include FT_FREETYPE_H

namespace VulkanTest
{
	DEFINE_RESOURCE(FontResource);

	FontResource::FontResource(const Path& path_, ResourceFactory& resFactory_) :
		BinaryResource(path_, resFactory_),
		face(nullptr)
	{
	}

	FontResource::~FontResource()
	{
		ASSERT(IsEmpty());
	}

	Font* FontResource::GetFont(I32 size)
	{
		PROFILE_FUNCTION();

		if (!WaitForLoaded())
			return nullptr;

		ScopedMutex lock(mutex);
		if (face == nullptr)
			return nullptr;

		for (auto font : fonts)
		{
			if (font->GetResource() == this && font->GetSize() == size)
				return font;
		}

		Font* ret = CJING_NEW(Font)(this, size);
		fonts.push_back(ret);
		return ret;
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

		fontData.Write(dataChunk->Data(), dataChunk->Size());

		FontResourceFactory* factory = static_cast<FontResourceFactory*>(&GetResourceFactory());
		const FT_Error error = FT_New_Memory_Face(factory->GetLibrary(), fontData.Data(), static_cast<FT_Long>(fontData.Size()), 0, &face);
		if (error)
		{
			face = nullptr;
			return false;
		}

		return true;
	}

	void FontResource::Unload()
	{
		for (auto font : fonts)
		{
			font->res = nullptr;
			font->DeleteObject();
		}
		fonts.clear();

		if (face != nullptr)
		{
			FT_Done_Face(face);
			face = nullptr;
		}

		fontData.Free();
	}
}