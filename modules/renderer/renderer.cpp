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
		U64 sortingKey;

		RenderBatch(ECS::EntityID mesh, ECS::EntityID obj, F32 distance)
		{
			ASSERT(mesh < 0x00FFFFFF);
			ASSERT(obj < 0x00FFFFFF);

			sortingKey = 0;
			sortingKey |= U64((U32)mesh & 0x00FFFFFF) << 40ull;
			sortingKey |= U64(ConvertFloatToHalf(distance) & 0xFFFF) << 24ull;
			sortingKey |= U64((U32)obj & 0x00FFFFFF) << 0ull;
		}

		inline float GetDistance() const
		{
			return ConvertHalfToFloat(HALF((sortingKey >> 24ull) & 0xFFFF));
		}

		inline ECS::EntityID GetMeshEntity() const
		{
			return ECS::EntityID((sortingKey >> 40ull) & 0x00FFFFFF);
		}

		inline ECS::EntityID GetInstanceEntity() const
		{
			return ECS::EntityID((sortingKey >> 0ull) & 0x00FFFFFF);
		}

		bool operator<(const RenderBatch& other) const
		{
			return sortingKey < other.sortingKey;
		}
	};

	struct RenderQueue
	{
		void SortOpaque()
		{
			std::sort(batches.begin(), batches.end(), std::less<RenderBatch>());
		}

		void Clear()
		{
			batches.clear();
		}

		void Add(ECS::EntityID mesh, ECS::EntityID obj, F32 distance)
		{
			batches.emplace(mesh, obj, distance);
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

	GPU::BlendState stockBlendStates[BSTYPE_COUNT] = {};
	GPU::RasterizerState stockRasterizerState[RSTYPE_COUNT] = {};
	GPU::DepthStencilState depthStencilStates[DSTYPE_COUNT] = {};
	GPU::Shader* shaders[SHADERTYPE_COUNT] = {};
	GPU::BufferPtr frameBuffer;

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
		stockBlendStates[BSTYPE_OPAQUE] = bd;

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
		stockBlendStates[BSTYPE_TRANSPARENT] = bd;

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
		stockRasterizerState[RSTYPE_FRONT] = rs;

		rs.cullMode = VK_CULL_MODE_FRONT_BIT;
		stockRasterizerState[RSTYPE_BACK] = rs;

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
		stockRasterizerState[RSTYPE_DOUBLE_SIDED] = rs;

		// DepthStencilStates
		GPU::DepthStencilState dsd;
		dsd.depthEnable = true;
		dsd.depthWriteMask = GPU::DEPTH_WRITE_MASK_ALL;
		dsd.depthFunc = VK_COMPARE_OP_GREATER;

		dsd.stencilEnable = true;
		dsd.stencilReadMask = 0;
		dsd.stencilWriteMask = 0xFF;
		dsd.frontFace.stencilFunc = VK_COMPARE_OP_ALWAYS;
		dsd.frontFace.stencilPassOp = VK_STENCIL_OP_REPLACE;
		dsd.frontFace.stencilFailOp = VK_STENCIL_OP_KEEP;
		dsd.frontFace.stencilDepthFailOp = VK_STENCIL_OP_KEEP;
		dsd.backFace.stencilFunc = VK_COMPARE_OP_ALWAYS;
		dsd.backFace.stencilPassOp = VK_STENCIL_OP_REPLACE;
		dsd.backFace.stencilFailOp = VK_STENCIL_OP_KEEP;
		dsd.backFace.stencilDepthFailOp = VK_STENCIL_OP_KEEP;
		depthStencilStates[DSTYPE_DEFAULT] = dsd;
	}

	GPU::Shader* PreloadShader(GPU::ShaderStage stage, const char* path, const GPU::ShaderVariantMap& defines = {})
	{
		GPU::DeviceVulkan& device = *GetDevice();
		return device.GetShaderManager().LoadShader(stage, path, defines);
	}

	void LoadShaders()
	{
		shaders[SHADERTYPE_VS_OBJECT] = PreloadShader(GPU::ShaderStage::VS, "objectVS.hlsl");
		shaders[SHADERTYPE_VS_VERTEXCOLOR] = PreloadShader(GPU::ShaderStage::VS, "vertexColorVS.hlsl");

		shaders[SHADERTYPE_PS_OBJECT] = PreloadShader(GPU::ShaderStage::PS, "objectPS.hlsl");
		shaders[SHADERTYPE_PS_VERTEXCOLOR] = PreloadShader(GPU::ShaderStage::PS, "vertexColorPS.hlsl");
	}

	void Renderer::Initialize(Engine& engine)
	{
		Logger::Info("Render initialized");

		InitStockStates();
		LoadShaders();

		auto device = GetDevice();
		GPU::BufferCreateInfo info = {};
		info.domain = GPU::BufferDomain::Device;
		info.size = sizeof(FrameCB);
		info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		frameBuffer = device->CreateBuffer(info, nullptr);
		device->SetName(*frameBuffer, "FrameBuffer");

		// Initialize resource factories
		ResourceManager& resManager = engine.GetResourceManager();
		textureFactory.Initialize(Texture::ResType, resManager);
		modelFactory.Initialize(Model::ResType, resManager);
		materialFactory.Initialize(Material::ResType, resManager);
	}

	void Renderer::Uninitialize()
	{
		frameBuffer.reset();

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

	const GPU::BlendState& GetBlendState(BlendStateTypes type)
	{
		return stockBlendStates[type];
	}
	
	const GPU::RasterizerState& GetRasterizerState(RasterizerStateTypes type)
	{
		return stockRasterizerState[type];
	}

	const GPU::DepthStencilState& GetDepthStencilState(DepthStencilStateType type)
	{
		return depthStencilStates[type];
	}

	const GPU::Shader* GetShader(ShaderType type)
	{
		return shaders[type];
	}

	void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta, FrameCB& frameCB)
	{
		ASSERT(visible.scene);
		frameCB.scene = visible.scene->GetShaderScene();
	}

	void UpdateRenderData(const Visibility& visible, const FrameCB& frameCB, GPU::CommandList& cmd)
	{
		ASSERT(visible.scene);
		cmd.BeginEvent("UpdateRenderData");

		// Update frame constbuffer
		cmd.UpdateBuffer(frameBuffer.get(), &frameCB, sizeof(frameCB));
		cmd.BufferBarrier(*frameBuffer,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_ACCESS_UNIFORM_READ_BIT);

		visible.scene->UpdateRenderData(cmd);

		cmd.EndEvent();
	}

	void BindCameraCB(const CameraComponent& camera, GPU::CommandList& cmd)
	{
		CameraCB cb;
		cb.viewProjection = camera.viewProjection;
		cmd.BindConstant(cb, 0, CBSLOT_RENDERER_CAMERA);
	}

	void BindCommonResources(GPU::CommandList& cmd)
	{
		auto heap = cmd.GetDevice().GetBindlessDescriptorHeap(GPU::BindlessReosurceType::StorageBuffer);
		if (heap != nullptr)
			cmd.SetBindless(1, heap->GetDescriptorSet());

		cmd.BindConstantBuffer(frameBuffer, 0, CBSLOT_RENDERER_FRAME, 0, sizeof(FrameCB));
	}

	void DrawMeshes(GPU::CommandList& cmd, const RenderQueue& queue, const Visibility& vis, RENDERPASS renderPass, U32 renderFlags)
	{
		if (queue.Empty())
			return;

		RenderScene* scene = vis.scene;
		cmd.BeginEvent("DrawMeshes");

		const size_t allocSize = queue.Size() * sizeof(ShaderMeshInstancePointer);
		GPU::BufferBlockAllocation allocation = cmd.AllocateStorageBuffer(allocSize);

		struct InstancedBatch
		{
			ECS::EntityID meshID = ECS::INVALID_ENTITY;
			uint32_t instanceCount = 0;
			uint32_t dataOffset = 0;
		} instancedBatch = {};

		auto FlushBatch = [&]()
		{
			MeshComponent* meshCmp = scene->GetComponent<MeshComponent>(instancedBatch.meshID);
			if (meshCmp == nullptr ||
				meshCmp->mesh == nullptr)
				return;

			Mesh& mesh = *meshCmp->mesh;
			cmd.BindIndexBuffer(mesh.generalBuffer, mesh.ib.offset, VK_INDEX_TYPE_UINT32);

			for (U32 subsetIndex = 0; subsetIndex < mesh.subsets.size(); subsetIndex++)
			{
				auto& subset = mesh.subsets[subsetIndex];
				if (subset.indexCount <= 0)
					continue;

				MaterialComponent* material = scene->GetComponent<MaterialComponent>(subset.materialID);

				ObjectPushConstants push;
				push.geometryIndex = meshCmp->geometryOffset + subsetIndex;
				push.materialIndex = material != nullptr ? material->materialIndex : 0;
				push.instance = allocation.bindless ? allocation.bindless->GetIndex() : -1;	// Pointer to ShaderInstancePointers
				push.instanceOffset = (U32)instancedBatch.dataOffset;

				cmd.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
				cmd.PushConstants(&push, 0, sizeof(push));
				cmd.SetDefaultOpaqueState();
				cmd.SetProgram(
					GetShader(ShaderType::SHADERTYPE_VS_OBJECT), 
					GetShader(ShaderType::SHADERTYPE_PS_OBJECT)
				);
				cmd.DrawIndexedInstanced(subset.indexCount, instancedBatch.instanceCount, subset.indexOffset, 0, 0);
			}
		};

		U32 instanceCount = 0;
		for (auto& batch : queue.batches)
		{
			const ECS::EntityID objID = batch.GetInstanceEntity();
			const ECS::EntityID meshID = batch.GetMeshEntity();

			ObjectComponent* obj = scene->GetComponent<ObjectComponent>(objID);
			if (meshID != instancedBatch.meshID)
			{
				FlushBatch();

				instancedBatch = {};
				instancedBatch.meshID = meshID;
				instancedBatch.dataOffset = allocation.offset + instanceCount * sizeof(ShaderMeshInstancePointer);
			}

			ShaderMeshInstancePointer data;
			data.instanceIndex = obj->index;
			memcpy((ShaderMeshInstancePointer*)allocation.data + instanceCount, &data, sizeof(ShaderMeshInstancePointer));

			instancedBatch.instanceCount++;
			instanceCount++;
		}

		FlushBatch();
		cmd.EndEvent();
	}

	void DrawScene(GPU::CommandList& cmd, const Visibility& vis)
	{
		RenderScene* scene = vis.scene;
		if (!scene)
			return;

		cmd.BeginEvent("DrawScene");

		BindCommonResources(cmd);

#if 0
		static thread_local RenderQueue queue;
#else
		RenderQueue queue;
#endif
		queue.Clear();
		for (auto objectID : vis.objects)
		{
			ObjectComponent* obj = scene->GetComponent<ObjectComponent>(objectID);
			if (obj == nullptr || obj->mesh == ECS::INVALID_ENTITY)
				continue;

			const F32 distance = Distance(vis.camera->eye, obj->center);
			queue.Add(obj->mesh, objectID, distance);
		}

		if (!queue.Empty())
		{
			queue.SortOpaque();
			DrawMeshes(cmd, queue, vis, RENDERPASS::RENDERPASS_MAIN, 0);
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
