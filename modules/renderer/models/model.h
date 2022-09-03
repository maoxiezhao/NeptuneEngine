#pragma once

#include "core\resource\binaryResource.h"
#include "core\streaming\streaming.h"
#include "renderer\materials\material.h"
#include "mesh.h"

namespace VulkanTest
{
#define MODEL_LOD_TO_CHUNK_INDEX(lod) (lod + 1)

	class ModelStreamTask;

	struct MaterialSlot
	{
		ResPtr<Material> material;
	};

	class VULKAN_TEST_API ModelLOD : public Object
	{
	public:
		bool Load(InputMemoryStream& input);
		void Unload();
		void Dispose();

		Array<Mesh>& GetMeshes() {
			return meshes;
		}

		F32 screenSize = 1.0f;

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

		static const I32 MAX_MODEL_LODS = 6;

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

		U32 GetLODsCount()const 
		{
			return modelLods.size();
		}

		I32 ClampLODIndex(I32 index) const
		{
			return std::clamp(index, HighestResidentLODIndex(), (I32)modelLods.size() - 1);
		}

		ModelLOD* GetModelLOD(I32 lodIndex) 
		{
			return &modelLods[lodIndex];
		}

		const ModelLOD* GetModelLOD(I32 lodIndex)const
		{
			return &modelLods[lodIndex];
		}

		Mesh* GetMesh(I32 lodIndex, I32 meshIndex) 
		{
			if (lodIndex < 0 || meshIndex < 0)
				return nullptr;
			return &modelLods[lodIndex].GetMeshes()[meshIndex];
		}

		I32 HighestResidentLODIndex() const 
		{
			return GetLODsCount() - loadedLODs;
		}

		const Array<MaterialSlot>& GetMaterials()const 
		{
			return materialSlots;
		}

		bool IsReady()const override;
		PickResult CastRayPick(const VECTOR& rayOrigin, const VECTOR& rayDirection, F32 tmin, F32 tmax);
		I32 CalculateModelLOD(F32x3 eye, F32x3 pos, F32 radius);

	protected:
		bool Init(ResourceInitData& initData)override;
		bool Load()override;
		void Unload() override;

		void GetLODData(I32 lodIndex, OutputMemoryStream& data) const;
		ContentLoadingTask* RequestLODDataAsync(I32 lodIndex);

	private:
		friend class ModelLOD;
		friend class ModelStreamTask;

		Model(const Model&) = delete;
		void operator=(const Model&) = delete;

	private:
		bool ParseMeshes(InputMemoryStream& mem);

		FileHeader header;
		I32 loadedLODs = 0;
		ModelStreamTask* streamTask = nullptr;
		Array<MaterialSlot> materialSlots;
		Array<ModelLOD> modelLods;
	};
}