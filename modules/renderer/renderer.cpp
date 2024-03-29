#include "renderer.h"
#include "shaderInterop.h"
#include "shaderInterop_renderer.h"
#include "shaderInterop_postprocess.h"
#include "gpu\vulkan\wsi.h"
#include "core\profiler\profiler.h"
#include "renderScene.h"
#include "renderPath3D.h"
#include "textureHelper.h"
#include "imageUtil.h"
#include "debugDraw.h"
#include "render2D\fontResource.h"
#include "render2D\render2D.h"
#include "render2D\fontManager.h"
#include "math\vMath_impl.hpp"

#include "content\resourceManager.h"
#include "content\binaryResource.h"
#include "content\resources\model.h"
#include "content\resources\material.h"
#include "content\resources\texture.h"

namespace VulkanTest
{

struct ShaderToCompile 
{
	Shader* shader;
	GPU::ShaderVariantMap defines;
	GPU::ShaderTemplateProgram* shaderTemplate;
	U64 hash = 0;
};

RendererService::~RendererService()
{
}

RendererService::RenderingServicesArray& RendererService::GetRenderingServices()
{
	static RenderingServicesArray Services;
	return Services;
}

void RendererService::Sort()
{
	auto& services = GetRenderingServices();
	std::sort(services.begin(), services.end(), [](RendererService* a, RendererService* b) {
		return a->order < b->order;
		});
}

RendererService::RendererService(const char* name_, I32 order_) :
	name(name_),
	order(order_)
{
	GetRenderingServices().push_back(this);
}

bool RendererService::Init(Engine& engine)
{
	return true;
}

void RendererService::OnInit(Engine& engine)
{
	// Sort services first
	Sort();

	// Init services from front to back
	auto& services = GetRenderingServices();
	for (I32 i = 0; i < (I32)services.size(); i++)
	{
		const auto service = services[i];
		Logger::Info("Initialize renderer service %s...", service->name);
		service->initialized = true;
		if (!service->Init(engine))
		{
			Logger::Error("Failed to initialize %s.", service->name);
			return;
		}
	}
}

void RendererService::Uninit()
{
}

void RendererService::OnUninit()
{
	auto& services = GetRenderingServices();
	for (I32 i = (I32)services.size() - 1; i >= 0; i--)
	{
		const auto service = services[i];
		if (service->initialized)
		{
			service->initialized = false;
			service->Uninit();
		}
	}
}

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
		const MeshComponent::MeshInfo* meshInfo;
		const ObjectComponent* objCmp;

		RenderBatch(const MeshComponent::MeshInfo* meshInfo_, const ObjectComponent* objCmp_, U32 instanceIndex, F32 distance) :
			meshInfo(meshInfo_),
			objCmp(objCmp_)
		{
			ASSERT(instanceIndex < 0x00FFFFFF);
			sortingKey = 0;
			sortingKey |= U64((U32)meshInfo->mesh->GetGUID().GetHash() & 0x00FFFFFF) << 40ull;
			sortingKey |= U64(ConvertFloatToHalf(distance) & 0xFFFF) << 24ull;
			sortingKey |= U64((U32)instanceIndex & 0x00FFFFFF) << 0ull;
		}

		inline float GetDistance() const
		{
			return ConvertHalfToFloat(HALF((sortingKey >> 24ull) & 0xFFFF));
		}

		inline const MeshComponent::MeshInfo* GetMeshInfo() const
		{
			return meshInfo;
		}
		
		inline const ObjectComponent* GetObjectComponent()const {
			return objCmp;
		}

		inline U32 GetInstanceIndex() const
		{
			return (sortingKey >> 0ull) & 0x00FFFFFF;
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

		void Add(const MeshComponent::MeshInfo* meshInfo, const ObjectComponent* objCmp, U32 instanceIndex, F32 distance)
		{
			batches.emplace(meshInfo, objCmp, instanceIndex, distance);
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

	GPU::BlendState stockBlendStates[BSTYPE_COUNT] = {};
	GPU::RasterizerState stockRasterizerState[RSTYPE_COUNT] = {};
	GPU::DepthStencilState depthStencilStates[DSTYPE_COUNT] = {};
	ResPtr<Shader> shaders[SHADERTYPE_COUNT] = {};
	GPU::BufferPtr frameBuffer;
	GPU::BufferPtr shaderLightBuffer;
	GPU::BindlessDescriptorPtr shaderLightBufferBindless;
	GPU::SamplerPtr samplers[SAMPLERTYPE_COUNT] = {};
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
		stockRasterizerState[RSTYPE_SKY] = rs;

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
		dsd.stencilEnable = false;
		dsd.depthWriteMask = GPU::DEPTH_WRITE_MASK_ZERO;
		dsd.depthFunc = VK_COMPARE_OP_EQUAL;
		depthStencilStates[DSTYPE_READEQUAL] = dsd;

		dsd.depthEnable = true;
		dsd.stencilEnable = false;
		dsd.depthWriteMask = GPU::DEPTH_WRITE_MASK_ZERO;
		dsd.depthFunc = VK_COMPARE_OP_GREATER_OR_EQUAL;
		depthStencilStates[DSTYPE_READ] = dsd;

		dsd.depthEnable = false;
		dsd.stencilEnable = false;
		depthStencilStates[DSTYPE_DISABLED] = dsd;

		// Samplers
		GPU::SamplerCreateInfo samplerInfo = {};
		samplerInfo.minLod = 0;
		samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
		samplerInfo.maxAnisotropy = 16.0f;
		samplerInfo.anisotropyEnable = true;
		samplerInfo.compareEnable = false;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplers[SAMPLERTYPE_OBJECT] = GPU::GPUDevice::Instance->RequestSampler(samplerInfo);
	}

	void LoadShaders()
	{
		shaders[SHADERTYPE_OBJECT] = ResourceManager::LoadResourceInternal<Shader>(Path("shaders/object"));
		shaders[SHADERTYPE_VERTEXCOLOR] = ResourceManager::LoadResourceInternal<Shader>(Path("shaders/vertexColor"));
		shaders[SHADERTYPE_POSTPROCESS_OUTLINE] = ResourceManager::LoadResourceInternal<Shader>(Path("shaders/outline"));
		shaders[SHADERTYPE_POSTPROCESS_BLUR_GAUSSIAN] = ResourceManager::LoadResourceInternal<Shader>(Path("shaders/blurGaussian"));
		shaders[SHADERTYPE_TILED_LIGHT_CULLING] = ResourceManager::LoadResourceInternal<Shader>(Path("shaders/lightCulling"));
		shaders[SHADERTYPE_MESHLET] = ResourceManager::LoadResourceInternal<Shader>(Path("shaders/meshlet"));
		shaders[SHADERTYPE_SKY] = ResourceManager::LoadResourceInternal<Shader>(Path("shaders/sky"));
	}

	void Renderer::Initialize(Engine& engine)
	{
		Logger::Info("Render initialized");

		// Reflect render scene
		RenderScene::Reflect();

		// Load builtin states
		InitStockStates();
		LoadShaders();

		// Create builtin constant buffers
		auto device = GPU::GPUDevice::Instance;
		GPU::BufferCreateInfo info = {};
		info.domain = GPU::BufferDomain::Device;
		info.size = sizeof(FrameCB);
		info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		frameBuffer = device->CreateBuffer(info, nullptr);
		device->SetName(*frameBuffer, "FrameBuffer");

		// Create shader lights buffer
		GPU::BufferCreateInfo lightBufferInfo = {};
		lightBufferInfo.domain = GPU::BufferDomain::Device;
		lightBufferInfo.size = sizeof(ShaderLight) * SHADER_ENTITY_COUNT;
		lightBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		shaderLightBuffer = device->CreateBuffer(lightBufferInfo, nullptr);
		device->SetName(*shaderLightBuffer, "ShaderLightBuffer");
		shaderLightBufferBindless = device->CreateBindlessStroageBuffer(*shaderLightBuffer, 0, shaderLightBuffer->GetCreateInfo().size);

		// Initialize renderer services
		RendererService::OnInit(engine);
	}

	void Renderer::Uninitialize()
	{
		// Release resources
		for (int i = 0; i < ARRAYSIZE(shaders); i++)
			shaders[i].reset();

		// Uninitialize renderder services
		RendererService::OnUninit();

		// Release frame buffer
		frameBuffer.reset();

		// Release shader light buffer
		shaderLightBuffer.reset();
		shaderLightBufferBindless.reset();

		// Release samplers
		for (int i = 0; i < ARRAYSIZE(samplers); i++)
			samplers[i].reset();

		rendererPlugin = nullptr;
		Logger::Info("Render uninitialized");
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

	ResPtr<Shader> GetShader(ShaderType type)
	{
		return shaders[type];
	}

	void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta, FrameCB& frameCB)
	{
		ASSERT(visible.scene);
		frameCB.scene = visible.scene->GetShaderScene();
		frameCB.lightOffset = 0;
		frameCB.lightCount = (uint)visible.lights.size();
		frameCB.bufferShaderLightsIndex = shaderLightBufferBindless->GetIndex();
		frameCB.objectSamplerIndex = samplers[SAMPLERTYPE_OBJECT]->GetOrCreateBindlesssIndex();
	}

	void UpdateRenderData(const Visibility& visible, const FrameCB& frameCB, GPU::CommandList& cmd)
	{
		ASSERT(visible.scene);
		PROFILE_FUNCTION();
		PROFILE_GPU("UpdateRenderData", cmd);

		cmd.BeginEvent("UpdateRenderData");

		// Update frame constbuffer
		cmd.UpdateBuffer(frameBuffer.get(), &frameCB, sizeof(frameCB));
		cmd.BufferBarrier(*frameBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT);

		// Update shader scene
		visible.scene->UpdateRenderData(cmd);

		// Update shader lights
		auto allocation = cmd.AllocateStorageBuffer(sizeof(ShaderLight) * SHADER_ENTITY_COUNT);
		ASSERT(allocation.data != nullptr);
		ShaderLight* shaderLights = (ShaderLight*)allocation.data;

		U32 lightCount = 0;
		for (auto lightEntity : visible.lights)
		{
			if (lightEntity == ECS::INVALID_ENTITY)
				continue;

			const LightComponent* light = lightEntity.Get<LightComponent>();
			if (light == nullptr)
				continue;

			ShaderLight shaderLight = {};
			shaderLight.SetType((uint)light->type);
			shaderLight.position = light->position;
			shaderLight.SetRange(light->range);

			F32x4 color = F32x4(light->color * light->intensity, 1.0f);
			shaderLight.SetColor(color);

			switch (light->type)
			{
			case LightComponent::DIRECTIONAL:
				shaderLight.SetDirection(light->direction);
				break;
			case LightComponent::POINT:
				break;
			case LightComponent::SPOT:
			{
				// https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual#inner-and-outer-cone-angles
				F32 outerConeAngleCos = std::cos(light->outerConeAngle);
				F32 lightAngleScale = 1.0f / std::max(0.001f, std::cos(light->innerConeAngle) - outerConeAngleCos);
				F32 lightAngleOffset = -outerConeAngleCos * lightAngleScale;

				shaderLight.SetConeAngleCos(outerConeAngleCos);
				shaderLight.SetAngleOffset(lightAngleOffset);
				shaderLight.SetAngleScale(lightAngleScale);
				shaderLight.SetDirection(light->direction);
			}
				break;
			default:
				ASSERT(false);
				break;
			}

			memcpy(shaderLights + lightCount, &shaderLight, sizeof(ShaderLight));
			lightCount++;
		}

		if (lightCount > 0)
		{
			cmd.CopyBuffer(
				*shaderLightBuffer,
				0,
				*allocation.buffer,
				allocation.offset,
				lightCount * sizeof(ShaderLight)
			);
			cmd.BufferBarrier(*shaderLightBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		}

		cmd.EndEvent();
	}

	void BindCameraCB(const CameraComponent& camera, GPU::CommandList& cmd)
	{
		CameraCB cb;
		cb.view = camera.view;
		cb.viewProjection = camera.viewProjection;
		cb.invProjection = camera.invProjection;
		cb.invViewProjection = camera.invViewProjection;
		cb.position = camera.eye;
		cb.zNear = camera.nearZ;
		cb.forward = camera.at;
		cb.zFar = camera.farZ;
		cb.resolution = U32x2((U32)camera.width, (U32)camera.height);
		cb.resolutionRcp = F32x2(1.0f / camera.width, 1.0f / camera.height);
		cb.cullingTileCount = GetLightCullingTileCount(cb.resolution);

		cb.textureDepthIndex = camera.textureDepthBindless ? camera.textureDepthBindless->GetIndex() : -1;
		cb.cullingTileBufferIndex = camera.bufferLightTileBindless ? camera.bufferLightTileBindless->GetIndex() : -1;
		cmd.BindConstant(cb, 0, CBSLOT_RENDERER_CAMERA);
	}

	void BindFrameCB(GPU::CommandList& cmd)
	{
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
			const MeshComponent::MeshInfo* meshInfo = nullptr;
			uint32_t instanceCount = 0;
			uint32_t dataOffset = 0;
			U8 stencilRef = 0;
		} 
		instancedBatch = {};

		auto FlushBatch = [&]()
		{
			if (instancedBatch.instanceCount <= 0)
				return;

			auto meshInfo = instancedBatch.meshInfo;
			const MaterialComponent* materialCmp = meshInfo->material.Get<MaterialComponent>();
			if (materialCmp == nullptr || materialCmp->materials.empty())
				return;

			Mesh& mesh = *meshInfo->mesh;
			cmd.BindIndexBuffer(mesh.generalBuffer, mesh.ib.offset, mesh.GetIndexFormat() == GPU::IndexBufferFormat::UINT32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);

			for (U32 subsetIndex = 0; subsetIndex < mesh.subsets.size(); subsetIndex++)
			{
				auto& subset = mesh.subsets[subsetIndex];
				if (subset.indexCount <= 0 || subset.materialIndex < 0)
					continue;

				Material* material = materialCmp->materials[subset.materialIndex];
				if (!material || !material->IsReady())
					continue;

				// Bind material
				MaterialShader::BindParameters bindParams = {};
				bindParams.cmd = &cmd;
				bindParams.renderPass = renderPass;
				material->Bind(bindParams);

				// StencilRef
				U8 stencilRef = instancedBatch.stencilRef;
				cmd.SetStencilRef(stencilRef, GPU::STENCIL_FACE_FRONT_AND_BACK);

				// PushConstants
				ObjectPushConstants push;
				push.geometryIndex = meshInfo->geometryOffset + subsetIndex;
				push.materialIndex = materialCmp->materialOffset + subset.materialIndex;
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
			U32 instanceIndex = batch.GetInstanceIndex();
			const auto* meshInfo = batch.GetMeshInfo();
			const ObjectComponent* obj = batch.GetObjectComponent();

			if (meshInfo != instancedBatch.meshInfo ||
				obj->stencilRef != instancedBatch.stencilRef)
			{
				FlushBatch();

				instancedBatch = {};
				instancedBatch.meshInfo = meshInfo;
				instancedBatch.dataOffset = allocation.offset + instanceCount * sizeof(ShaderMeshInstancePointer);
				instancedBatch.stencilRef = obj->stencilRef;
			}

			ShaderMeshInstancePointer data;
			data.instanceIndex = instanceIndex;
			memcpy((ShaderMeshInstancePointer*)allocation.data + instanceCount, &data, sizeof(ShaderMeshInstancePointer));

			instancedBatch.instanceCount++;
			instanceCount++;
		}

		FlushBatch();
		cmd.EndEvent();
	}

	void DrawScene(const Visibility& vis, RENDERPASS pass, GPU::CommandList& cmd)
	{
		RenderScene* scene = vis.scene;
		if (!scene)
			return;

		cmd.BeginEvent("DrawScene");

		BindFrameCB(cmd);

#if 0
		static thread_local RenderQueue queue;
#else
		RenderQueue queue;
#endif
		queue.Clear();
		for (auto objectID : vis.objects)
		{
			const ObjectComponent* obj = objectID.Get<ObjectComponent>();
			if (obj == nullptr || obj->mesh == ECS::INVALID_ENTITY)
				continue;

			const MeshComponent* mesh = obj->mesh.Get<MeshComponent>();
			if (mesh == nullptr || obj->lodIndex >= mesh->lodsCount || !mesh->model->IsReady())
				continue;

			auto& lodMeshes = mesh->meshes[obj->lodIndex];
			for (U32 i = 0; i < lodMeshes.size(); i++)
			{
				const F32 distance = Distance(vis.camera->eye, obj->center);
				queue.Add(&lodMeshes[i], obj, obj->instanceOffset + i, distance);
			}
		}

		if (!queue.Empty())
		{
			queue.SortOpaque();
			DrawMeshes(cmd, queue, vis, pass, 0);
		}

		cmd.EndEvent();
	}

	void DrawSky(RenderScene& scene, GPU::CommandList& cmd)
	{
		// TODO Use DrawSky to replace this function
		cmd.BeginEvent("DrawSky");
		cmd.SetBlendState(GetBlendState(BSTYPE_OPAQUE));
		cmd.SetDepthStencilState(GetDepthStencilState(DSTYPE_READ));
		cmd.SetRasterizerState(GetRasterizerState(RSTYPE_SKY));
		cmd.SetProgram(
			Renderer::GetShader(ShaderType::SHADERTYPE_SKY)->GetVS("VS"),
			Renderer::GetShader(ShaderType::SHADERTYPE_SKY)->GetPS("PS_Sky")
		);
		cmd.Draw(3);
		cmd.EndEvent();
	}

	I32 ComputeModelLOD(const Model* model, F32x3 eye, F32x3 pos, F32 radius)
	{
		F32 distSq = DistanceSquared(eye, pos);
		const float radiussq = radius * radius;
		if (distSq < radiussq)
			return 0;

		if (model->GetLODsCount() <= 1)
			return 0;

		for (int i = model->GetLODsCount() - 1; i >= 0; i--)
		{
			const auto& lod = model->GetModelLOD(i);
			if (lod->screenSize >= distSq)
				return i;
		}
		return 0;
	}

	U32x2 GetVisibilityTileCount(const U32x2& resolution)
	{
		return U32x2(
			(resolution.x + VISIBILITY_BLOCKSIZE - 1) / VISIBILITY_BLOCKSIZE,
			(resolution.y + VISIBILITY_BLOCKSIZE - 1) / VISIBILITY_BLOCKSIZE);
	}

	U32x3 GetLightCullingTileCount(const U32x2& resolution)
	{
		return U32x3(
			(resolution.x + TILED_CULLING_BLOCK_SIZE - 1) / TILED_CULLING_BLOCK_SIZE,
			(resolution.y + TILED_CULLING_BLOCK_SIZE - 1) / TILED_CULLING_BLOCK_SIZE,
			1);
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
