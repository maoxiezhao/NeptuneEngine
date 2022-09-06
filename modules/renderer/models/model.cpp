#include "model.h"
#include "core\profiler\profiler.h"
#include "core\resource\resourceManager.h"
#include "core\streaming\streamingHandler.h"
#include "renderer\renderer.h"
#include "core\threading\threadPoolTask.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Model);

	const U32 Model::FILE_MAGIC = 0x5f4c4d4f;
	const U32 Model::FILE_VERSION = 0x01;

	namespace
	{
		static U8 GetIndexBySemantic(Mesh::AttributeSemantic semantic)
		{
			switch (semantic)
			{
			case Mesh::AttributeSemantic::POSITION: return 0;
			case Mesh::AttributeSemantic::NORMAL: return 1;
			case Mesh::AttributeSemantic::TEXCOORD0: return 2;
			case Mesh::AttributeSemantic::COLOR0: return 3;
			case Mesh::AttributeSemantic::TANGENT: return 4;
			default: ASSERT(false); return 0;
			}
		}

		static VkFormat formatMap[(U32)Mesh::AttributeType::COUNT][4] = {
			{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT}, // F32,
			{VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT}, // I32,
		};
		static VkFormat GetFormat(Mesh::AttributeType type, U8 compCount)
		{
			return formatMap[(U32)type][compCount - 1];
		}

		static int GetTypeSize(Mesh::AttributeType type)
		{
			switch (type)
			{
			case Mesh::AttributeType::F32: return 4;
			case Mesh::AttributeType::I32: return 4;
			default: ASSERT(false); return 0;
			}
		}
	}

	class ModelStreamTask : public ThreadPoolTask
	{
	public:
		ModelStreamTask(Model* model_, I32 lodIndex_) :
			modelPtr(model_),
			lodIndex(lodIndex_),
			lock(model_->storage->Lock())
		{}

		bool Run()override
		{
			ResPtr<Model> model = modelPtr.get();
			if (model == nullptr)
				return false;

			OutputMemoryStream lodData;
			model->GetLODData(lodIndex, lodData);
			if (lodData.Empty())
			{
				Logger::Warning("Missing lod data chunk");
				return false;
			}

			InputMemoryStream input(lodData);
			if (!model->modelLods[lodIndex].Load(input))
			{
				Logger::Warning("Failed to load lod %d from model %s", lodIndex, model->GetPath().c_str());
				return false;
			}

			model->loadedLODs++;
			return true;
		}

		void OnEnd()override
		{
			if (modelPtr)
			{
				ASSERT(modelPtr->streamTask == this);
				modelPtr->streamTask = nullptr;
				modelPtr.reset();
			}

			lock.Release();
			Task::OnEnd();
		}

	private:
		ResPtr<Model> modelPtr;
		I32 lodIndex;
		ResourceStorage::StorageLock lock;
	};

	bool ModelLOD::Load(InputMemoryStream& input)
	{
		verticesCount = 0;
		for (int i = 0; i < (I32)meshes.size(); i++)
		{
			auto& mesh = meshes[i];

			// Read attributes
			U32 attrCount;
			input.Read(attrCount);

			Mesh::AttributeSemantic semantics[GPU::InputLayout::MAX_ATTRIBUTES];
			for (auto& i : semantics)
				i = Mesh::AttributeSemantic::NONE;

			// Read layout
			GPU::InputLayout layout = {};
			U8 offset = 0;
			for (U32 j = 0; j < attrCount; j++)
			{
				Mesh::AttributeType type;
				U8 compCount;
				input.Read(semantics[j]);
				input.Read(type);
				input.Read(compCount);

				const U8 attrIdx = GetIndexBySemantic(semantics[j]);
				const auto format = GetFormat(type, compCount);
				layout.AddAttribute(format, offset);

				offset += GetTypeSize(type) * compCount;
			}

			// Read indices
			I32 indexSize;
			I32 indicesCount;
			input.Read(indexSize);

			// Index only U32 now
			// if (indexSize != 2 && indexSize != 4)
			if (indexSize != 4)
				return false;
			input.Read(indicesCount);
			if (indicesCount <= 0)
				return false;

			mesh.indices.resize(indicesCount);
			input.Read(mesh.indices.data(), sizeof(U32) * indicesCount);

			// Read vertex datas
			U32 vertexCount;
			input.Read(vertexCount);
			for (U32 i = 0; i < layout.attributeCount; i++)
			{
				switch (semantics[i])
				{
				case Mesh::AttributeSemantic::POSITION:
					mesh.vertexPos.resize(vertexCount);
					input.Read(mesh.vertexPos.data(), sizeof(F32x3) * vertexCount);
					break;
				case Mesh::AttributeSemantic::NORMAL:
					mesh.vertexNor.resize(vertexCount);
					input.Read(mesh.vertexNor.data(), sizeof(F32x3) * vertexCount);
					break;
				case Mesh::AttributeSemantic::TEXCOORD0:
					mesh.vertexUV.resize(vertexCount);
					input.Read(mesh.vertexUV.data(), sizeof(F32x2) * vertexCount);
					break;
				case Mesh::AttributeSemantic::TANGENT:
					mesh.vertexTangents.resize(vertexCount);
					input.Read(mesh.vertexTangents.data(), sizeof(F32x4) * vertexCount);
					break;
				default:
					ASSERT(false);
					break;
				}
			}

			// Create mesh render datas
			if (!mesh.Load())
			{
				Logger::Warning("Failed to initialize mesh %s", mesh.name.c_str());
				return false;
			}
		}
		return true;
	}

	void ModelLOD::Unload()
	{
		for (auto& mesh : meshes)
			mesh.Unload();
	}

	void ModelLOD::Dispose()
	{
		model = nullptr;
		meshes.clear();
	}

	Model::Model(const ResourceInfo& info, ResourceManager& resManager) :
		BinaryResource(info, resManager),
		StreamableResource(StreamingHandlers::Instance()->Model()),
		header()
	{
	}

	Model::~Model()
	{
		ASSERT(streamTask == nullptr);
	}

	I32 Model::GetMaxResidency() const
	{
		return (I32)modelLods.size();
	}

	I32 Model::GetCurrentResidency() const
	{
		return loadedLODs;
	}

	bool Model::ShouldUpdate() const
	{
		return IsReady() && streamTask == nullptr;
	}

	Task* Model::CreateStreamingTask(I32 residency)
	{
		ScopedMutex lock(mutex);
		ASSERT(streamTask == nullptr && residency >= 0 && residency <= (I32)modelLods.size());

		Task* task = nullptr;
		const I32 lodCount = residency - GetCurrentResidency();
		if (lodCount > 0)
		{
			ASSERT(lodCount == 1);
			I32 lodIndex = HighestResidentLODIndex() - 1;

			// Request data chunk of lods async
			task = RequestLODDataAsync(lodIndex);

			// Load from lod data after chunk loaded
			streamTask = CJING_NEW(ModelStreamTask)(this, lodIndex);
			if (task)
				task->SetNextTask(streamTask);
			else
				task = streamTask;
		}
		else
		{
			I32 lodIndex = HighestResidentLODIndex();
			for (; lodIndex < ((I32)modelLods.size() - residency); lodIndex++)
				modelLods[lodIndex].Unload();
			loadedLODs = residency;
		}

		return task;
	}

	void Model::CancelStreamingTask()
	{
		if (streamTask != nullptr)
		{
			streamTask->Cancel();
			streamTask = nullptr;
		}
	}

	bool Model::IsReady() const
	{
		return Resource::IsReady() && loadedLODs > 0;
	}

	PickResult Model::CastRayPick(const VECTOR& rayOrigin, const VECTOR& rayDirection, F32 tmin, F32 tmax)
	{
		PickResult ret;
		auto& meshes = modelLods[0].GetMeshes();
		for (auto& mesh : meshes)
		{
			PickResult hit = mesh.CastRayPick(rayOrigin, rayDirection, tmin, tmax);
			if (hit.isHit && hit.distance < ret.distance)
				ret = hit;
		}
		return ret;
	}

	I32 Model::CalculateModelLOD(F32x3 eye, F32x3 pos, F32 radius)
	{
		I32 lodIndex = Renderer::ComputeModelLOD(this, eye, pos, radius);
		if (lodIndex == -1)
			return -1;

		lodIndex = ClampLODIndex(lodIndex);
		return lodIndex;
	}

	bool Model::Init(ResourceInitData& initData)
	{
		InputMemoryStream inputMem(initData.customData);
		inputMem.Read<FileHeader>(header);
		if (header.magic != FILE_MAGIC)
		{
			Logger::Warning("Unsupported model file %s", GetPath());
			return false;
		}
		if (header.version != FILE_VERSION)
		{
			Logger::Warning("Unsupported version of model %s", GetPath());
			return false;
		}

		return true;
	}

	bool Model::Load()
	{
		PROFILE_FUNCTION();

		const auto dataChunk = GetChunk(0);
		if (dataChunk == nullptr || !dataChunk->IsLoaded())
			return false;
		InputMemoryStream input(dataChunk->Data(), dataChunk->Size());

		// Read material slots
		I32 materialCount = 0;
		input.Read(materialCount);
		if (materialCount <= 0)
			return false;
		materialSlots.resize(materialCount);

		auto& resManager = GetResourceManager();
		for (I32 i = 0; i < materialCount; i++)
		{
			// Mateiral guid
			Guid matGuid;
			input.Read(matGuid);

			// Material name;
			I32 nameLenght;
			input.Read(nameLenght);
			char matName[MAX_PATH_LENGTH];
			matName[nameLenght] = 0;
			input.Read(matName, nameLenght);

			auto material = resManager.LoadResource<Material>(matGuid);
			if (!material)
			{
				Logger::Error("Faield to load material of model %s", matName);
				return false;
			}
			AddDependency(*material);

			materialSlots[i].name = matName;
			materialSlots[i].material = std::move(material);
		}

		// Load lods
		U8 lods = 0;
		input.Read(lods);
		if (lods == 0 || lods > MAX_MODEL_LODS)
			return false;
		modelLods.resize(lods);

		// Parse lods
		for (I32 lodIndex = 0; lodIndex < lods; lodIndex++)
		{
			auto& lod = modelLods[lodIndex];
			lod.model = this;

			U16 meshCount = 0;
			input.Read(meshCount);
			if (meshCount == 0)
				return false;
			lod.meshes.resize(meshCount);

			for (int meshIndex = 0; meshIndex < meshCount; meshIndex++)
			{
				// Read mesh name
				I32 strSize;
				input.Read(strSize);
				char meshName[MAX_PATH_LENGTH];
				meshName[strSize] = 0;
				input.Read(meshName, strSize);

				// Read mesh aabb
				AABB aabb;
				input.ReadAABB(&aabb);

				// Initialize mesh
				lod.meshes[meshIndex].Init(meshName, this, lodIndex, meshIndex, aabb);

				// Read mesh subsets
				auto& mesh = lod.meshes[meshIndex];
				I32 subsetCount = 0;
				input.Read(subsetCount);
				if (subsetCount <= 0)
					return false;
				mesh.subsets.resize(subsetCount);

				for (int j = 0; j < subsetCount; j++)
				{
					Mesh::MeshSubset& subset = mesh.subsets[j];

					// Material slot index
					I32 materialIndex;
					input.Read(materialIndex);
					if (materialIndex < 0 || materialIndex >= (I32)materialSlots.size())
						Logger::Warning("Invalid material index in lod %d from model %s", lodIndex, GetPath().c_str());
					subset.materialIndex = materialIndex;

					input.Read(subset.indexOffset);
					input.Read(subset.indexCount);
				}
			}
		}
		
		// Start streaming
		StartStreaming(true);

		return true;
	}

	void Model::Unload()
	{
		// End streaming
		if (streamTask != nullptr)
		{
			streamTask->Cancel();
			streamTask = nullptr;
		}

		for (auto& mateiral : materialSlots)
		{
			if (mateiral.material)
				RemoveDependency(*mateiral.material);
		}		
		materialSlots.clear();

		for (auto& lod : modelLods)
			lod.Dispose();
		modelLods.clear();

		loadedLODs = 0;
	}

	void Model::CancelStreaming()
	{
		CancelStreamingTask();
	}

	void Model::GetLODData(I32 lodIndex, OutputMemoryStream& data) const
	{
		const I32 chunkIndex = MODEL_LOD_TO_CHUNK_INDEX(lodIndex);
		GetChunkData(chunkIndex, data);
	}

	ContentLoadingTask* Model::RequestLODDataAsync(I32 lodIndex)
	{
		const I32 chunkIndex = MODEL_LOD_TO_CHUNK_INDEX(lodIndex);
		return (ContentLoadingTask*)RequestChunkData(chunkIndex);
	}
}