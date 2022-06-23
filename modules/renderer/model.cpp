#include "model.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Model);

	const U32 Model::FILE_MAGIC = 0x5f4c4d4f;
	const U32 Model::FILE_VERSION = 0x01;

	Model::Model(const Path& path_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_)
	{
	}

	Model::~Model()
	{
	}

	bool Model::OnLoaded(U64 size, const U8* mem)
	{
		return false;
	}

	void Model::OnUnLoaded()
	{
	}
}