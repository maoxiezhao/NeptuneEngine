#include "renderer.h"
#include "shaderInterop.h"
#include "renderer\renderScene.h"
#include "renderer\renderPath3D.h"
#include "gpu\vulkan\wsi.h"
#include "core\utils\profiler.h"
#include "core\resource\resourceManager.h"
#include "model.h"
#include "material.h"
#include "texture.h"

namespace VulkanTest
{

struct RendererPluginImpl : public RendererPlugin
{
public:
	RendererPluginImpl(Engine& engine_): engine(engine_) {}

	virtual ~RendererPluginImpl()
	{
		Renderer::Uninitialize();
	}

	void Initialize() override
	{
		Renderer::Initialize(engine);
	}

	GPU::DeviceVulkan* GetDevice() override
	{
		return engine.GetWSI().GetDevice();
	}

	RenderScene* GetScene() override
	{
		return scene;
	}

	const char* GetName() const override
	{
		return "Renderer";
	}

	void CreateScene(World& world) override 
	{
		UniquePtr<RenderScene> newScene = RenderScene::CreateScene(*this, engine, world);
		world.AddScene(newScene.Move());
	}

private:
	Engine& engine;
	RenderPath3D defaultPath;
	RenderScene* scene = nullptr;
};

namespace Renderer
{
	struct RenderBatch
	{
		ECS::EntityID mesh;
		U64 sortingKey;
	};

	struct RenderQueue
	{
		void Sort()
		{
		}

		void Clear()
		{
		}

		bool Empty()const
		{
			return batches.empty();
		}

		size_t Size()const
		{
			return batches.size();
		}

		Array<RenderBatch> batches;
	};

	template <typename T>
	struct RenderResourceFactory : public ResourceFactory
	{
	protected:
		virtual Resource* CreateResource(const Path& path) override
		{
			return CJING_NEW(T)(path, *this);
		}

		virtual void DestroyResource(Resource* res) override
		{
			CJING_DELETE(res);
		}
	};
	RenderResourceFactory<Texture> textureFactory;
	RenderResourceFactory<Model> modelFactory;
	MaterialFactory materialFactory;

	GPU::BlendState stockBlendStates[BlendStateType_Count] = {};
	GPU::RasterizerState stockRasterizerState[RasterizerStateType_Count] = {};

	RendererPlugin* rendererPlugin = nullptr;

	void InitStockStates()
	{
		// Blend states
		GPU::BlendState bd;
		bd.renderTarget[0].blendEnable = false;
		bd.renderTarget[0].srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
		bd.renderTarget[0].destBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		bd.renderTarget[0].blendOp = VK_BLEND_OP_MAX;
		bd.renderTarget[0].srcBlendAlpha = VK_BLEND_FACTOR_ONE;
		bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_ZERO;
		bd.renderTarget[0].blendOpAlpha = VK_BLEND_OP_ADD;
		bd.renderTarget[0].renderTargetWriteMask = GPU::COLOR_WRITE_ENABLE_ALL;
		bd.alphaToCoverageEnable = false;
		bd.independentBlendEnable = false;
		stockBlendStates[BlendStateType_Opaque] = bd;

		bd.renderTarget[0].blendEnable = true;
		bd.renderTarget[0].srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
		bd.renderTarget[0].destBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		bd.renderTarget[0].blendOp = VK_BLEND_OP_ADD;
		bd.renderTarget[0].srcBlendAlpha = VK_BLEND_FACTOR_ONE;
		bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		bd.renderTarget[0].blendOpAlpha = VK_BLEND_OP_ADD;
		bd.renderTarget[0].renderTargetWriteMask = GPU::COLOR_WRITE_ENABLE_ALL;
		bd.alphaToCoverageEnable = false;
		bd.independentBlendEnable = false;
		stockBlendStates[BlendStateType_Transparent] = bd;

		// Rasterizer states
		GPU::RasterizerState rs;
		rs.fillMode = GPU::FILL_SOLID;
		rs.cullMode = VK_CULL_MODE_BACK_BIT;
		rs.frontCounterClockwise = true;
		rs.depthBias = 0;
		rs.depthBiasClamp = 0;
		rs.slopeScaledDepthBias = 0;
		rs.depthClipEnable = false;
		rs.multisampleEnable = false;
		rs.antialiasedLineEnable = false;
		rs.conservativeRasterizationEnable = false;
		stockRasterizerState[RasterizerStateType_Front] = rs;

		rs.cullMode = VK_CULL_MODE_FRONT_BIT;
		stockRasterizerState[RasterizerStateType_Back] = rs;

		rs.fillMode = GPU::FILL_SOLID;
		rs.cullMode = VK_CULL_MODE_NONE;
		rs.frontCounterClockwise = false;
		rs.depthBias = 0;
		rs.depthBiasClamp = 0;
		rs.slopeScaledDepthBias = 0;
		rs.depthClipEnable = false;
		rs.multisampleEnable = false;
		rs.antialiasedLineEnable = false;
		rs.conservativeRasterizationEnable = false;
		stockRasterizerState[RasterizerStateType_DoubleSided] = rs;
	}

	void LoadShaders()
	{
	}

	// Test
	ResPtr<Model> testModel;
	GPU::BufferPtr geometryBuffer;
	U32 geometryArraySize = 0;
	GPU::BufferPtr geometryUploadBuffer[2];
	GPU::BindlessDescriptorPtr bindlessGeometryBuffer;

	GPU::BufferPtr materialBuffer;
	U32 materialArraySize = 0;
	GPU::BufferPtr materialUploadBuffer[2];
	GPU::BindlessDescriptorPtr bindlessMaterialBuffer;

	ShaderSceneCB sceneCB;

	void Renderer::Initialize(Engine& engine)
	{
		Logger::Info("Render initialized");

		InitStockStates();
		LoadShaders();

		// Initialize resource factories
		ResourceManager& resManager = engine.GetResourceManager();
		textureFactory.Initialize(Texture::ResType, resManager);
		modelFactory.Initialize(Model::ResType, resManager);
		materialFactory.Initialize(Material::ResType, resManager);

		// test
		testModel = resManager.LoadResourcePtr<Model>(Path("models/cornellbox.obj"));
	}

	void Renderer::Uninitialize()
	{
		testModel.reset();
		geometryBuffer.reset();
		geometryUploadBuffer[0].reset();
		geometryUploadBuffer[1].reset();
		bindlessGeometryBuffer.reset();

		materialBuffer.reset();
		materialUploadBuffer[0].reset();
		materialUploadBuffer[1].reset();
		bindlessMaterialBuffer.reset();

		// Uninitialize resource factories
		materialFactory.Uninitialize();
		modelFactory.Uninitialize();
		textureFactory.Uninitialize();

		rendererPlugin = nullptr;
		Logger::Info("Render uninitialized");
	}

	GPU::DeviceVulkan* GetDevice()
	{
		ASSERT(rendererPlugin != nullptr);
		return rendererPlugin->GetDevice();
	}

	RenderScene* GetScene()
	{
		ASSERT(rendererPlugin != nullptr);
		return rendererPlugin->GetScene();
	}

	const GPU::BlendState& GetBlendState(BlendStateTypes types)
	{
		return stockBlendStates[types];
	}
	
	const GPU::RasterizerState& GetRasterizerState(RasterizerStateTypes types)
	{
		return stockRasterizerState[types];
	}

	void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta)
	{
		UpdateGeometryBuffer();
	}

	void UpdateRenderData(const Visibility& visible, GPU::CommandList& cmd)
	{
	}

	void BindCameraCB(const CameraComponent& camera, GPU::CommandList& cmd)
	{
		CameraCB cb;
		cb.viewProjection = camera.viewProjection;
		cmd.BindConstant(cb, 0, CBSLOT_RENDERER_CAMERA);
	}

	void DrawMeshes(GPU::CommandList& cmd, const RenderQueue& queue, RENDERPASS renderPass, U32 renderFlags)
	{
		if (queue.Empty())
			return;

		cmd.BeginEvent("DrawMeshes");

		RenderScene* scene = Renderer::GetScene();
		for (auto& batch : queue.batches)
		{
			MeshComponent* mesh = scene->GetComponent<MeshComponent>(batch.mesh);
			if (mesh == nullptr)
				continue;

			cmd.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
			cmd.BindIndexBuffer(mesh->ibo, 0, VK_INDEX_TYPE_UINT32);
			cmd.BindVertexBuffer(mesh->vboPos, 0, 0, sizeof(F32x3), VK_VERTEX_INPUT_RATE_VERTEX);
			cmd.SetVertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);

			for (U32 index = 0; index < mesh->subsets.size(); index++)
			{
				MeshComponent::MeshSubset& subset = mesh->subsets[index];
				if (subset.indexCount == 0)
					continue;

				// Set rendering state
				cmd.SetProgram("objectVS.hlsl", "objectPS.hlsl");
				cmd.SetDefaultOpaqueState();
				cmd.DrawIndexed(subset.indexCount, subset.indexOffset, 0);
			}
		}

		cmd.EndEvent();
	}

	void DrawScene(GPU::CommandList& cmd, const Visibility& vis)
	{
		cmd.BeginEvent("DrawScene");

		RenderScene* secne = Renderer::GetScene();
		RenderQueue queue;
		for (auto meshID : vis.objects)
		{
			MeshComponent* mesh = secne->GetComponent<MeshComponent>(meshID);
			if (mesh == nullptr)
				continue;

			RenderBatch batch = {};
			batch.mesh = meshID;
			queue.batches.push_back(batch);
		}

		if (!queue.Empty())
			DrawMeshes(cmd, queue, RENDERPASS::RENDERPASS_MAIN, 0);

		cmd.EndEvent();
	}

	/// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// <Test>
	/// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void BindCommonResources(GPU::CommandList& cmd)
	{
		cmd.BindConstant(sceneCB, 0, 2);

		auto heap = cmd.GetDevice().GetBindlessDescriptorHeap(GPU::BindlessReosurceType::StorageBuffer);
		if (heap != nullptr)
			cmd.SetBindless(1, heap->GetDescriptorSet());
	}

	void UpdateGeometryBuffer()
	{
		if (!testModel || !testModel->IsReady())
			return;

		GPU::DeviceVulkan& device = *GetDevice();

		// Create material buffer
		Array<Material*> materials;
		for (int i = 0; i < testModel->GetMeshCount(); i++)
		{
			auto& mesh = testModel->GetMesh(i);
			for (auto& subset : mesh.subsets)
			{
				if (subset.material)
					materials.push_back(subset.material.get());
			}
		}

		materialArraySize = materials.size();

		U32 materialBufferSize = materialArraySize * sizeof(ShaderMaterial);
		if (materialArraySize > 0 && (!materialBuffer || materialBuffer->GetCreateInfo().size < materialBufferSize))
		{
			GPU::BufferCreateInfo info = {};
			info.domain = GPU::BufferDomain::Device;
			info.size = materialBufferSize * 2;
			info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
				VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
				VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			materialBuffer = device.CreateBuffer(info, nullptr);
			device.SetName(*materialBuffer, "matiralBuffer");

			info.domain = GPU::BufferDomain::LinkedDeviceHost;
			info.usage = 0;

			for (int i = 0; i < ARRAYSIZE(materialUploadBuffer); i++)
			{
				materialUploadBuffer[i] = device.CreateBuffer(info, nullptr);
				device.SetName(*materialUploadBuffer[i], "materialUploadBuffer");
			}

			bindlessMaterialBuffer = device.CreateBindlessStroageBuffer(*materialBuffer, 0, materialBuffer->GetCreateInfo().size);
		}

		// Update upload material buffer
		ShaderMaterial* materialMapped = (ShaderMaterial*)device.MapBuffer(*materialUploadBuffer[device.GetFrameIndex()], GPU::MEMORY_ACCESS_WRITE_BIT);
		if (materialMapped == nullptr)
			return;
		for (int i = 0; i < materials.size(); i++)
		{
			auto& material = materials[i];

			ShaderMaterial shaderMaterial;
			shaderMaterial.baseColor = material->GetColor().ToFloat4();

			memcpy(materialMapped + i, &shaderMaterial, sizeof(ShaderMaterial));
		}
		device.UnmapBuffer(*materialUploadBuffer[device.GetFrameIndex()], GPU::MEMORY_ACCESS_WRITE_BIT);

		// Create geometry buffer
		geometryArraySize = 0;
		for (int i = 0; i < testModel->GetMeshCount(); i++)
		{
			Mesh& mesh = testModel->GetMesh(i);
			mesh.geometryOffset = geometryArraySize;
			geometryArraySize += mesh.subsets.size();
		}

		U32 geometryBufferSize = geometryArraySize * sizeof(ShaderGeometry);
		if (geometryArraySize > 0 && (!geometryBuffer || geometryBuffer->GetCreateInfo().size < geometryBufferSize))
		{
			GPU::BufferCreateInfo info = {};
			info.domain = GPU::BufferDomain::Device;
			info.size = geometryBufferSize * 2;
			info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
				VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
				VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			geometryBuffer = device.CreateBuffer(info, nullptr);
			device.SetName(*geometryBuffer, "geometryBuffer");

			info.domain = GPU::BufferDomain::LinkedDeviceHost;
			info.usage = 0;

			for (int i = 0; i < ARRAYSIZE(geometryUploadBuffer); i++)
			{
				geometryUploadBuffer[i] = device.CreateBuffer(info, nullptr);
				device.SetName(*geometryUploadBuffer[i], "geometryUploadBuffer");
			}

			bindlessGeometryBuffer = device.CreateBindlessStroageBuffer(*geometryBuffer, 0, geometryBuffer->GetCreateInfo().size);
		}

		// Update upload geometry buffer
		ShaderGeometry* geometryMapped = (ShaderGeometry*)device.MapBuffer(*geometryUploadBuffer[device.GetFrameIndex()], GPU::MEMORY_ACCESS_WRITE_BIT);
		if (geometryMapped == nullptr)
			return;

		for (int i = 0; i < testModel->GetMeshCount(); i++)
		{
			Mesh& mesh = testModel->GetMesh(i);

			ShaderGeometry geometry;
			geometry.vbPos = mesh.vbPos.srv->GetIndex();
			geometry.vbNor = mesh.vbNor.srv->GetIndex();
			geometry.vbUVs = mesh.vbUVs.srv->GetIndex();
			geometry.ib = 0;

			U32 subsetIndex = 0;
			for (auto& subset : mesh.subsets)
			{
				U32 materialIndex = 0;
				for (auto material : materials)
				{
					if (subset.material.get() == material)
						break;

					materialIndex++;
				}
				subset.materialIndex = materialIndex;

				memcpy(geometryMapped + mesh.geometryOffset + subsetIndex, &geometry, sizeof(ShaderGeometry));
				subsetIndex++;
			}
		}

		device.UnmapBuffer(*geometryUploadBuffer[device.GetFrameIndex()], GPU::MEMORY_ACCESS_WRITE_BIT);

		// Update scene
		sceneCB.geometrybuffer = bindlessGeometryBuffer->GetIndex();
		sceneCB.materialbuffer = bindlessMaterialBuffer->GetIndex();
	}

	void UpdateRenderData(GPU::CommandList& cmd)
	{
		auto& device = cmd.GetDevice();
		if (geometryBuffer && geometryArraySize > 0)
		{
			auto uploadBuffer = geometryUploadBuffer[device.GetFrameIndex()];
			if (uploadBuffer)
			{
				cmd.CopyBuffer(geometryBuffer, uploadBuffer);
				cmd.BufferBarrier(*geometryBuffer,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_ACCESS_SHADER_READ_BIT);
			}
		}

		if (materialBuffer && materialArraySize > 0)
		{
			auto uploadBuffer = materialUploadBuffer[device.GetFrameIndex()];
			if (uploadBuffer)
			{
				cmd.CopyBuffer(materialBuffer, uploadBuffer);
				cmd.BufferBarrier(*materialBuffer,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_ACCESS_SHADER_READ_BIT);
			}
		}
	}

	void DrawModel(GPU::CommandList& cmd)
	{
		if (!testModel || !testModel->IsReady())
			return;

		if (!geometryBuffer || !materialBuffer)
			return;

		Model& model = *testModel;
		I32 modelCount = model.GetMeshCount();
		if (modelCount <= 0)
			return;

		auto& device = cmd.GetDevice();
		cmd.BeginEvent("DrawMeshes");

		BindCommonResources(cmd);
		cmd.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		for (int i = 0; i < modelCount; i++)
		{
			Mesh& mesh = model.GetMesh(i);

			cmd.BindIndexBuffer(mesh.generalBuffer, mesh.ib.offset, VK_INDEX_TYPE_UINT32);

			for (U32 subsetIndex = 0; subsetIndex < mesh.subsets.size(); subsetIndex++)
			{
				auto& subset = mesh.subsets[subsetIndex];
				if (subset.indexCount <= 0)
					continue;

				ObjectPushConstants push;
				push.geometryIndex = mesh.geometryOffset + subsetIndex;
				push.materialIndex = subset.materialIndex;

				cmd.PushConstants(&push, 0, sizeof(push));
				cmd.SetDefaultOpaqueState();
				cmd.SetProgram("objectVS.hlsl", "objectPS.hlsl");
				cmd.DrawIndexed(subset.indexCount, subset.indexOffset, 0);
			}
		}

		cmd.EndEvent();
	}

	RendererPlugin* CreatePlugin(Engine& engine)
	{
		rendererPlugin = CJING_NEW(RendererPluginImpl)(engine);
		return rendererPlugin;
	}
}
}
