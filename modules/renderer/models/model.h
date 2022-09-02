#pragma once

#include "core\resource\binaryResource.h"
#include "core\streaming\streaming.h"
#include "renderer\materials\material.h"
#include "mesh.h"

namespace VulkanTest
{
	class VULKAN_TEST_API ModelLod : public Object
	{
	public:
		bool Load(InputMemoryStream& input);
		void Unload();
		void Dispose();

	private:
		friend class Model;

		Model* model = nullptr;
		U32 verticesCount = 0;
		Array<Mesh> meshes;
	};

	class VULKAN_TEST_API Model final : public BinaryResource, public StreamableResource
	{
	public:
		DECLARE_RESOURCE(Model);

#pragma pack(1)
		struct FileHeader
		{
			U32 magic;
			U32 version;
		};
#pragma pack()
		static const U32 FILE_MAGIC;
		static const U32 FILE_VERSION;

		Model(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Model();
	
		// Residency
		I32 GetMaxResidency() const override;
		I32 GetCurrentResidency() const override;

		// Check current resource should be update
		bool ShouldUpdate()const override;

		// Create streaming task
		Task* CreateStreamingTask(I32 residency) override;

		Mesh& GetMesh(U32 index) { 
			return meshes[index];
		}
		const Mesh& GetMesh(U32 index) const { 
			return meshes[index]; 
		}
		I32 GetMeshCount() const { 
			return meshes.size();
		}

	protected:
		bool Init(ResourceInitData& initData)override;
		bool Load()override;
		void Unload() override;

	private:
		Model(const Model&) = delete;
		void operator=(const Model&) = delete;

		bool ParseMaterials(InputMemoryStream& mem);
		bool ParseMeshes(InputMemoryStream& mem);

		FileHeader header;
		Array<Mesh> meshes;
		Array<ModelLod> lods;
	};
}