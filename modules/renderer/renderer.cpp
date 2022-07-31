#include "renderer.h"
#include "shaderInterop.h"
#include "shaderInterop_renderer.h"
#include "shaderInterop_postprocess.h"
#include "gpu\vulkan\wsi.h"
#include "core\utils\profiler.h"
#include "core\resource\resourceManager.h"
#include "renderScene.h"
#include "renderPath3D.h"
#include "model.h"
#include "material.h"
#include "texture.h"
#include "textureHelper.h"
#include "imageUtil.h"

namespace VulkanTest
{

struct ShaderToCompile 
{
	Shader* shader;
	GPU::ShaderVariantMap defines;
	GPU::ShaderTemplateProgram* shaderTemplate;
	U64 hash = 0;
};

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

	Engine& GetEngine() override
	{
		return engine;
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
	Mutex shaderMutex;
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

	RenderResourceFactory<Shader> shaderFactory;
	RenderResourceFactory<Texture> textureFactory;
	RenderResourceFactory<Model> modelFactory;
	MaterialFactory materialFactory;

	GPU::BlendState stockBlendStates[BSTYPE_COUNT] = {};
	GPU::RasterizerState stockRasterizerState[RSTYPE_COUNT] = {};
	GPU::DepthStencilState depthStencilStates[DSTYPE_COUNT] = {};
	ResPtr<Shader> shaders[SHADERTYPE_COUNT] = {};
	GPU::BufferPtr frameBuffer;
	GPU::PipelineStateDesc objectPipelineStates
		[RENDERPASS_COUNT]
		[BLENDMODE_COUNT]
		[OBJECT_DOUBLESIDED_COUNT];
	bool isObjectPipelineStatesInited = false;

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
		stockBlendStates[BSTYPE_TRANSPARENT] = bd;

		bd.renderTarget[0].blendEnable = true;
		bd.renderTarget[0].srcBlend = VK_BLEND_FACTOR_ONE;
		bd.renderTarget[0].destBlend = VK_BLEND_FACTOR_SRC_ALPHA;
		bd.renderTarget[0].blendOp = VK_BLEND_OP_ADD;
		bd.renderTarget[0].srcBlendAlpha = VK_BLEND_FACTOR_ONE;
		bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_SRC_ALPHA;
		bd.renderTarget[0].blendOpAlpha = VK_BLEND_OP_ADD;
		bd.renderTarget[0].renderTargetWriteMask = GPU::COLOR_WRITE_ENABLE_ALL;
		stockBlendStates[BSTYPE_PREMULTIPLIED] = bd;

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

		dsd.depthEnable = true;
		dsd.depthWriteMask = GPU::DEPTH_WRITE_MASK_ZERO;
		dsd.depthFunc = VK_COMPARE_OP_EQUAL;
		depthStencilStates[DSTYPE_READEQUAL] = dsd;

		dsd.depthEnable = false;
		dsd.stencilEnable = false;
		depthStencilStates[DSTYPE_DISABLED] = dsd;
	}

	GPU::Shader* PreloadShader(GPU::ShaderStage stage, const char* path, const GPU::ShaderVariantMap& defines)
	{
		GPU::DeviceVulkan& device = *GetDevice();
		return device.GetShaderManager().LoadShader(stage, path, defines);
	}

	void LoadShaders()
	{
		auto resManager = rendererPlugin->GetEngine().GetResourceManager();
		shaders[SHADERTYPE_OBJECT] = resManager.LoadResourcePtr<Shader>(Path("shaders/object.shd"));
		shaders[SHADERTYPE_VERTEXCOLOR] = resManager.LoadResourcePtr<Shader>(Path("shaders/vertexColor.shd"));
		shaders[SHADERTYPE_POSTPROCESS_OUTLINE] = resManager.LoadResourcePtr<Shader>(Path("shaders/outline.shd"));
		shaders[SHADERTYPE_POSTPROCESS_BLUR_GAUSSIAN] = resManager.LoadResourcePtr<Shader>(Path("shaders/blurGaussian.shd"));
	}

	GPU::Shader* GetVSShader(RENDERPASS renderPass)
	{
		switch (renderPass)
		{
		case RENDERPASS_MAIN:
			return shaders[SHADERTYPE_OBJECT]->GetVS("VS", 0);
		case RENDERPASS_PREPASS:
			return shaders[SHADERTYPE_OBJECT]->GetVS("VS", 1);
		default:
			return nullptr;
		}
	}

	GPU::Shader* GetPSShader(RENDERPASS renderPass)
	{
		switch (renderPass)
		{
		case RENDERPASS_MAIN:
			return shaders[SHADERTYPE_OBJECT]->GetPS("PS", 0);
		case RENDERPASS_PREPASS:
			return shaders[SHADERTYPE_OBJECT]->GetPS("PS_Depth", 0);
		default:
			return nullptr;
		}
	}

	void LoadPipelineStates()
	{
		for (I32 renderPass = 0; renderPass < RENDERPASS_COUNT; ++renderPass)
		{
			for (I32 blendMode = 0; blendMode < BLENDMODE_COUNT; ++blendMode)
			{
				for (I32 doublesided = 0; doublesided < OBJECT_DOUBLESIDED_COUNT; ++doublesided)
				{
					GPU::PipelineStateDesc pipeline = {};
					memset(&pipeline, 0, sizeof(pipeline));

					// Shaders
					const bool transparency = blendMode != BLENDMODE_OPAQUE;
					pipeline.shaders[(I32)GPU::ShaderStage::VS] = GetVSShader((RENDERPASS)renderPass);
					pipeline.shaders[(I32)GPU::ShaderStage::PS] = GetPSShader((RENDERPASS)renderPass);
					
					// Blend states
					switch (blendMode)
					{
					case BLENDMODE_OPAQUE:
						pipeline.blendState = GetBlendState(BlendStateTypes::BSTYPE_OPAQUE);
						break;
					case BLENDMODE_ALPHA:
						pipeline.blendState = GetBlendState(BlendStateTypes::BSTYPE_TRANSPARENT);
						break;
					case BLENDMODE_PREMULTIPLIED:
						pipeline.blendState = GetBlendState(BlendStateTypes::BSTYPE_PREMULTIPLIED);
						break;
					default:
						ASSERT(false);
						break;
					}

					// DepthStencilStates
					switch (renderPass)
					{
					case RENDERPASS_MAIN:
						pipeline.depthStencilState = GetDepthStencilState(DepthStencilStateType::DSTYPE_READEQUAL);
						break;
					default:
						pipeline.depthStencilState = GetDepthStencilState(DepthStencilStateType::DSTYPE_DEFAULT);
						break;
					}

					// RasterizerStates
					switch (doublesided)
					{
					case OBJECT_DOUBLESIDED_FRONTSIDE:
						pipeline.rasterizerState = GetRasterizerState(RasterizerStateTypes::RSTYPE_FRONT);
						break;
					case OBJECT_DOUBLESIDED_ENABLED:
						pipeline.rasterizerState = GetRasterizerState(RasterizerStateTypes::RSTYPE_DOUBLE_SIDED);
						break;
					case OBJECT_DOUBLESIDED_BACKSIDE:
						pipeline.rasterizerState = GetRasterizerState(RasterizerStateTypes::RSTYPE_BACK);
						break;
					default:
						ASSERT(false);
						break;
					}
	
					objectPipelineStates[renderPass][blendMode][doublesided] = pipeline;
				}
			}
		}
	}

	void Renderer::Initialize(Engine& engine)
	{
		Logger::Info("Render initialized");

		// Initialize resource factories
		ResourceManager& resManager = engine.GetResourceManager();
		shaderFactory.Initialize(Shader::ResType, resManager);
		textureFactory.Initialize(Texture::ResType, resManager);
		modelFactory.Initialize(Model::ResType, resManager);
		materialFactory.Initialize(Material::ResType, resManager);

		// Load built-in states
		InitStockStates();
		LoadShaders();

		// Create built-in constant buffers
		auto device = GetDevice();
		GPU::BufferCreateInfo info = {};
		info.domain = GPU::BufferDomain::Device;
		info.size = sizeof(FrameCB);
		info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		frameBuffer = device->CreateBuffer(info, nullptr);
		device->SetName(*frameBuffer, "FrameBuffer");

		// Initialize image util
		ImageUtil::Initialize(resManager);

		// Initialize texture helper
		TextureHelper::Initialize(&resManager);
	}

	void Renderer::Uninitialize()
	{
		// Release resources
		for (int i = 0; i < ARRAYSIZE(shaders); i++)
			shaders[i].reset();

		ImageUtil::Uninitialize();
		TextureHelper::Uninitialize();

		frameBuffer.reset();

		// Uninitialize resource factories
		materialFactory.Uninitialize();
		modelFactory.Uninitialize();
		textureFactory.Uninitialize();
		shaderFactory.Uninitialize();

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

	Shader* GetShader(ShaderType type)
	{
		return shaders[type].get();
	}

	const GPU::PipelineStateDesc& GetObjectPipelineState(RENDERPASS renderPass, BlendMode blendMode, ObjectDoubleSided doublesided)
	{
		return objectPipelineStates[renderPass][blendMode][doublesided];
	}

	void DrawDebugObjects(const RenderScene& scene, const CameraComponent& camera, GPU::CommandList& cmd)
	{
	}

	void DrawBox(const FMat4x4& boxMatrix, const F32x4& color)
	{
	}

	void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta, FrameCB& frameCB)
	{
		ASSERT(visible.scene);
		frameCB.scene = visible.scene->GetShaderScene();

		// Update object pipeline states
		if (!isObjectPipelineStatesInited && GetShader(SHADERTYPE_OBJECT)->IsReady())
		{
			isObjectPipelineStatesInited = true;
			LoadPipelineStates();
		}
		else if (isObjectPipelineStatesInited && GetShader(SHADERTYPE_OBJECT)->IsEmpty())
		{
			isObjectPipelineStatesInited = false;
		}
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
		auto BindCommonBindless = [&](GPU::BindlessReosurceType type, U32 set) {
			auto heap = cmd.GetDevice().GetBindlessDescriptorHeap(type);
			if (heap != nullptr)
				cmd.SetBindless(set, heap->GetDescriptorSet());
		};
		BindCommonBindless(GPU::BindlessReosurceType::StorageBuffer, 1);
		BindCommonBindless(GPU::BindlessReosurceType::SampledImage, 2);

		cmd.BindConstantBuffer(frameBuffer, 0, CBSLOT_RENDERER_FRAME, 0, sizeof(FrameCB));
	}

	Ray GetPickRay(const F32x2& screenPos, const CameraComponent& camera)
	{
		F32 w = camera.width;
		F32 h = camera.height;

		MATRIX V = LoadFMat4x4(camera.view);
		MATRIX P = LoadFMat4x4(camera.projection);
		MATRIX W = MatrixIdentity();
		VECTOR lineStart = Vector3Unproject(XMVectorSet(screenPos.x, screenPos.y, 1, 1), 0.0f, 0.0f, w, h, 0.0f, 1.0f, P, V, W);
		VECTOR lineEnd = Vector3Unproject(XMVectorSet(screenPos.x, screenPos.y, 0, 1), 0.0f, 0.0f, w, h, 0.0f, 1.0f, P, V, W);
		VECTOR rayDirection = Vector3Normalize(VectorSubtract(lineEnd, lineStart));
		return Ray(StoreF32x3(lineStart), StoreF32x3(rayDirection));
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
			U8 stencilRef = 0;
		} instancedBatch = {};

		auto FlushBatch = [&]()
		{
			if (instancedBatch.instanceCount <= 0)
				return;

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
				if (!material || !material->material)
					continue;

				BlendMode blendMode = material->material->GetBlendMode();
				ObjectDoubleSided doubleSided = material->material->IsDoubleSided() ? OBJECT_DOUBLESIDED_ENABLED : OBJECT_DOUBLESIDED_FRONTSIDE;
				cmd.SetPipelineState(GetObjectPipelineState(renderPass, blendMode, doubleSided));

				// StencilRef
				U8 stencilRef = instancedBatch.stencilRef;
				cmd.SetStencilRef(stencilRef, GPU::STENCIL_FACE_FRONT_AND_BACK);

				// PushConstants
				ObjectPushConstants push;
				push.geometryIndex = meshCmp->geometryOffset + subsetIndex;
				push.materialIndex = material != nullptr ? material->materialIndex : 0;
				push.instance = allocation.bindless ? allocation.bindless->GetIndex() : -1;	// Pointer to ShaderInstancePointers
				push.instanceOffset = (U32)instancedBatch.dataOffset;

				cmd.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
				cmd.PushConstants(&push, 0, sizeof(push));
				cmd.DrawIndexedInstanced(subset.indexCount, instancedBatch.instanceCount, subset.indexOffset, 0, 0);
			}
		};

		U32 instanceCount = 0;
		for (auto& batch : queue.batches)
		{
			const ECS::EntityID objID = batch.GetInstanceEntity();
			const ECS::EntityID meshID = batch.GetMeshEntity();

			ObjectComponent* obj = scene->GetComponent<ObjectComponent>(objID);
			if (meshID != instancedBatch.meshID ||
				obj->stencilRef != instancedBatch.stencilRef)
			{
				FlushBatch();

				instancedBatch = {};
				instancedBatch.meshID = meshID;
				instancedBatch.dataOffset = allocation.offset + instanceCount * sizeof(ShaderMeshInstancePointer);
				instancedBatch.stencilRef = obj->stencilRef;
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

	void DrawScene(GPU::CommandList& cmd, const Visibility& vis, RENDERPASS pass)
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
			DrawMeshes(cmd, queue, vis, pass, 0);
		}

		cmd.EndEvent();
	}

	void SetupPostprocessBlurGaussian(RenderGraph& graph, const String& input, String& out, const AttachmentInfo& attchment)
	{
		// Replace 2D Gaussian blur with 1D Gaussian blur twice
		auto& blurPass = graph.AddRenderPass("BlurPass", RenderGraphQueueFlag::Compute);
		auto& readRes = blurPass.ReadTexture(input.c_str());
		auto& tempRes = blurPass.WriteStorageTexture("rtBlurTemp", attchment, "rtBlurTemp");
		tempRes.SetImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT);

		auto& output = blurPass.WriteStorageTexture("rtBlur", attchment);
		blurPass.SetBuildCallback([&](GPU::CommandList& cmd) {
			auto& read = graph.GetPhysicalTexture(readRes);
			auto& temp = graph.GetPhysicalTexture(tempRes);
			auto& out = graph.GetPhysicalTexture(output);

			cmd.SetProgram(Renderer::GetShader(SHADERTYPE_POSTPROCESS_BLUR_GAUSSIAN)->GetCS("CS"));

			PostprocessPushConstants push;
			push.resolution = {
				out.GetImage()->GetCreateInfo().width,
				out.GetImage()->GetCreateInfo().height
			};
			push.resolution_rcp = {
				1.0f / push.resolution.x,
				1.0f / push.resolution.y,
			};

			// Horizontal:
			push.params0.x = 1.0f;
			push.params0.y = 0.0f;

			cmd.PushConstants(&push, 0, sizeof(push));
			cmd.SetTexture(0, 0, read);
			cmd.SetStorageTexture(0, 0, temp);

			cmd.Dispatch(
				(push.resolution.x + POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT - 1) / POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT,
				push.resolution.y,
				1
			);

			cmd.ImageBarrier(*temp.GetImage(),
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT);
			temp.GetImage()->SetLayoutType(GPU::ImageLayoutType::Optimal);

			// Vertical:
			push.params0.x = 0.0f;
			push.params0.y = 1.0f;

			cmd.PushConstants(&push, 0, sizeof(push));
			cmd.SetTexture(0, 0, temp);
			cmd.SetStorageTexture(0, 0, out);
			cmd.Dispatch(
				push.resolution.x,
				(push.resolution.y + POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT - 1) / POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT,
				1
			);

			cmd.ImageBarrier(*temp.GetImage(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);
			temp.GetImage()->SetLayoutType(GPU::ImageLayoutType::General);
		});
		out = "rtBlur";
	}

	void PostprocessOutline(GPU::CommandList& cmd, const GPU::ImageView& texture, F32 threshold, F32 thickness, const F32x4& color)
	{
		cmd.SetRasterizerState(GetRasterizerState(RSTYPE_DOUBLE_SIDED));
		cmd.SetBlendState(GetBlendState(BSTYPE_TRANSPARENT));
		cmd.SetDepthStencilState(GetDepthStencilState(DSTYPE_DISABLED));
		cmd.SetTexture(0, 0, texture);
		cmd.SetProgram(
			GetShader(SHADERTYPE_POSTPROCESS_OUTLINE)->GetVS("VS"),
			GetShader(SHADERTYPE_POSTPROCESS_OUTLINE)->GetPS("PS")
		);

		PostprocessPushConstants push;
		push.resolution = {
			texture.GetImage()->GetCreateInfo().width,
			texture.GetImage()->GetCreateInfo().height
		};
		push.resolution_rcp = {
			1.0f / push.resolution.x,
			1.0f / push.resolution.y,
		};
		push.params0.x = threshold;
		push.params0.y = thickness;
		push.params1 = color;
		cmd.PushConstants(&push, 0, sizeof(push));

		cmd.Draw(3);
	}

	RendererPlugin* CreatePlugin(Engine& engine)
	{
		rendererPlugin = CJING_NEW(RendererPluginImpl)(engine);
		return rendererPlugin;
	}
}
}
