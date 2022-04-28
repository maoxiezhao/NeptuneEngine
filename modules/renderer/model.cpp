#include "model.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Model);

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