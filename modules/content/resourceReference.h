#pragma once

#include "resource.h"
#include "core\collections\hashMap.h"
#include "core\serialization\serialization.h"

namespace VulkanTest
{
	class VULKAN_TEST_API ResourceReferenceBase
	{
	public:
		ResourceReferenceBase() = default;
		~ResourceReferenceBase();

		HashValue GetHashValue() const {
			return resource ? resource->GetPath().GetHashValue() : 0;
		}

	protected:
		void OnSet(Resource* res);
		void OnLoaded(Resource* res);
		void OnUnloaded(Resource* res);

		Resource* resource = nullptr;
	};

	template<typename T>
	class VULKAN_TEST_API ResourceReference : public ResourceReferenceBase
	{
	public:
		ResourceReference()
		{
		}

		~ResourceReference()
		{
		}

		ResourceReference(T* resource)
		{
			OnSet(resource);
		}

		ResourceReference(const ResourceReference& other)
		{
			OnSet(other.get());
		}

		ResourceReference(ResourceReference&& other)
		{
			OnSet(other.get());
			other.OnSet(nullptr);
		}

		ResourceReference& operator=(ResourceReference&& other)
		{
			if (&other != this)
			{
				OnSet(other.get());
				other.OnSet(nullptr);
			}
			return *this;
		}

		ResourceReference& operator=(const ResourceReference& other)
		{
			OnSet(other.get());
			return *this;
		}

		ResourceReference& operator=(T* other)
		{
			OnSet(other);
			return *this;
		}

		FORCE_INLINE bool operator==(T* other)
		{
			return resource == other;
		}

		FORCE_INLINE ResourceReference& operator=(const Guid& id)
		{
			OnSet(LoadResource(T::ResType, id));
			return *this;
		}

		FORCE_INLINE bool operator==(const ResourceReference& other)
		{
			return resource == other.resource;
		}

		FORCE_INLINE bool operator!=(T* other)
		{
			return resource != other;
		}

		FORCE_INLINE bool operator!=(const ResourceReference& other)
		{
			return resource != other.resource;
		}

		FORCE_INLINE operator T* () const
		{
			return (T*)resource;
		}

		FORCE_INLINE operator bool() const
		{
			return resource != nullptr;
		}

		FORCE_INLINE T* operator->() const
		{
			return (T*)resource;
		}

		FORCE_INLINE Guid GetGuid()const 
		{
			return resource ? resource->GetGUID() : Guid::Empty;
		}

		FORCE_INLINE T* get() const
		{
			return (T*)resource;
		}

		void set(T* res)
		{
			OnSet(res);
		}

		void reset()
		{
			OnSet(nullptr);
		}
	};

	template<typename T>
	using ResPtr = ResourceReference<T>;

	template<typename T>
	struct HashMapHashFunc<ResourceReference<T>>
	{
		static U32 get(const ResourceReference<T>& key)
		{
			return key.GetHashValue();
		}
	};

	class VULKAN_TEST_API WeakResourceReferenceBase
	{
	public:
	public:
		WeakResourceReferenceBase() = default;
		~WeakResourceReferenceBase();

		HashValue GetHashValue() const {
			return resource ? resource->GetPath().GetHashValue() : 0;
		}

	protected:
		void OnSet(Resource* res);
		void OnUnloaded(Resource* res);

		Resource* resource = nullptr;
	};

	template<typename T>
	class VULKAN_TEST_API WeakResourceReference : public WeakResourceReferenceBase
	{
	public:
		WeakResourceReference()
		{
		}

		~WeakResourceReference()
		{
		}

		WeakResourceReference(T* resource)
		{
			OnSet(resource);
		}

		WeakResourceReference(const WeakResourceReference& other)
		{
			OnSet(other.get());
		}

		WeakResourceReference(WeakResourceReference&& other)
		{
			OnSet(other.get());
			other.OnSet(nullptr);
		}

		WeakResourceReference& operator=(WeakResourceReference&& other)
		{
			if (&other != this)
			{
				OnSet(other.get());
				other.OnSet(nullptr);
			}
			return *this;
		}

		WeakResourceReference& operator=(const WeakResourceReference& other)
		{
			OnSet(other.get());
			return *this;
		}

		WeakResourceReference& operator=(T* other)
		{
			OnSet(other);
			return *this;
		}

		FORCE_INLINE bool operator==(T* other)
		{
			return resource == other;
		}

		FORCE_INLINE bool operator==(const WeakResourceReference& other)
		{
			return resource == other.resource;
		}

		FORCE_INLINE bool operator!=(T* other)
		{
			return resource != other;
		}

		FORCE_INLINE bool operator!=(const WeakResourceReference& other)
		{
			return resource != other.resource;
		}

		FORCE_INLINE operator T* () const
		{
			return (T*)resource;
		}

		FORCE_INLINE operator bool() const
		{
			return resource != nullptr;
		}

		FORCE_INLINE T* operator->() const
		{
			return (T*)resource;
		}

		FORCE_INLINE T* get() const
		{
			return (T*)resource;
		}

		void set(T* res)
		{
			OnSet(res);
		}

		void reset()
		{
			OnSet(nullptr);
		}
	};

	template<typename T>
	using WeakResPtr = WeakResourceReference<T>;

	template<typename T>
	struct HashMapHashFunc<WeakResourceReference<T>>
	{
		static U32 get(const WeakResourceReference<T>& key)
		{
			return key.GetHashValue();
		}
	};

	namespace Serialization
	{
		template<typename T>
		struct SerializeTypeNormalMapping<ResourceReference<T>>
		{
			static void Serialize(ISerializable::SerializeStream& stream, const ResourceReference<T>& v, const void* otherObj)
			{
				stream.Guid(v.GetGuid());
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, ResourceReference<T>& v)
			{
				v = DeserializeGuid(stream);
			}

			static bool ShouldSerialize(const ResourceReference<T>& v, const void* obj)
			{
				return !obj || v.get() != ((ResourceReference<T>*)obj)->get();
			}
		};
	}
}