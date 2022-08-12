#include "resourceReference.h"

namespace VulkanTest
{
	ResourceReferenceBase::~ResourceReferenceBase()
	{
		if (resource)
		{
			resource->OnLoadedCallback.Unbind<&ResourceReferenceBase::OnLoaded>(this);
			resource->OnUnloadedCallback.Unbind<&ResourceReferenceBase::OnUnloaded>(this);
			resource->RemoveReference();
		}
	}

	void ResourceReferenceBase::OnSet(Resource* res)
	{
		if (resource != res)
		{
			if (resource != nullptr)
			{
				resource->OnLoadedCallback.Unbind<&ResourceReferenceBase::OnLoaded>(this);
				resource->OnUnloadedCallback.Unbind<&ResourceReferenceBase::OnUnloaded>(this);
				resource->RemoveReference();
			}

			resource = res;

			if (resource != nullptr)
			{
				resource->AddReference();
				resource->OnLoadedCallback.Bind<&ResourceReferenceBase::OnLoaded>(this);
				resource->OnUnloadedCallback.Bind<&ResourceReferenceBase::OnUnloaded>(this);
			}
		}
	}

	void ResourceReferenceBase::OnLoaded(Resource* res)
	{
		if (res != resource)
			return;
	}

	void ResourceReferenceBase::OnUnloaded(Resource* res)
	{
		// Set nullptr when holding resource is unloaded
		if (res != resource)
			return;
		OnSet(nullptr);
	}

	WeakResourceReferenceBase::~WeakResourceReferenceBase()
	{
		if (resource)
			resource->OnUnloadedCallback.Unbind<&WeakResourceReferenceBase::OnUnloaded>(this);
	}

	void WeakResourceReferenceBase::OnSet(Resource* res)
	{
		if (resource != res)
		{
			if (resource != nullptr)
				resource->OnUnloadedCallback.Unbind<&WeakResourceReferenceBase::OnUnloaded>(this);

			resource = res;

			if (resource != nullptr)
				resource->OnUnloadedCallback.Bind<&WeakResourceReferenceBase::OnUnloaded>(this);
		}
	}

	void WeakResourceReferenceBase::OnUnloaded(Resource* res)
	{
		// Set nullptr when holding resource is unloaded
		if (res != resource)
			return;

		resource->OnUnloadedCallback.Unbind<&WeakResourceReferenceBase::OnUnloaded>(this);
		resource = nullptr;
	}
}