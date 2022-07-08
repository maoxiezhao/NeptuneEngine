#include "RenderScene.h"
#include "renderer.h"
#include "culling.h"
#include "gpu\vulkan\wsi.h"
#include "core\scene\reflection.h"

namespace VulkanTest
{
    void CameraComponent::UpdateCamera()
    {
        projection = StoreFMat4x4(MatrixPerspectiveFovLH(fov, width / height, farZ, nearZ)); // reverse zbuffer!

        VECTOR _Eye = LoadF32x3(eye);
        VECTOR _At  = LoadF32x3(at);
        VECTOR _Up  = LoadF32x3(up);

        MATRIX _V = MatrixLookToLH(_Eye, _At, _Up);
        MATRIX _P = LoadFMat4x4(projection);
        MATRIX _VP = MatrixMultiply(_V, _P);

        view = StoreFMat4x4(_V);
        viewProjection = StoreFMat4x4(_VP);

        frustum.Compute(LoadFMat4x4(viewProjection));
    }

    void CameraComponent::UpdateTransform(const Transform& transform)
    {
        MATRIX mat = LoadFMat4x4(transform.world);
        eye = StoreF32x3(Vector3Transform(VectorSet(0, 0, 0, 1), mat));
        at = StoreF32x3(Vector3Normalize(Vector3TransformNormal(XMVectorSet(0, 0, 1, 0), mat)));
        up = StoreF32x3(Vector3Normalize(Vector3TransformNormal(XMVectorSet(0, 1, 0, 0), mat)));
    }

    class RenderSceneImpl : public RenderScene
    {
    public:
        Engine& engine;
        World& world;
        RendererPlugin& rendererPlugin;
        std::vector<ISystem*> systems;
        CameraComponent mainCamera;
        UniquePtr<CullingSystem> cullingSystem;

        EntityMap<Model*> modelEntityMap;
        ECS::Query<MeshComponent> meshQuery;

        // Runtime rendering infos
        GPU::BufferPtr geometryBuffer;
        U32 geometryArraySize = 0;
        ShaderGeometry* geometryMapped = nullptr;
        GPU::BufferPtr geometryUploadBuffer[2];
        GPU::BindlessDescriptorPtr bindlessGeometryBuffer;

        GPU::BufferPtr materialBuffer;
        U32 materialArraySize = 0;
        GPU::BufferPtr materialUploadBuffer[2];
        GPU::BindlessDescriptorPtr bindlessMaterialBuffer;

        ShaderSceneCB sceneCB;

    public:
        RenderSceneImpl(RendererPlugin& rendererPlugin_, Engine& engine_, World& world_) :
            rendererPlugin(rendererPlugin_),
            engine(engine_),
            world(world_)
        {
            // Reflect render scene
            RenderScene::Reflect(&world);

            cullingSystem = CullingSystem::Create();

            meshQuery = world.CreateQuery<MeshComponent>().Build();
            world.SetComponenetOnRemoved<ModelComponent>([&](ECS::EntityID entity, ModelComponent& model) {
                if (model.model)
                    modelEntityMap.erase(model.model.get());
            });
        }

        virtual ~RenderSceneImpl()
        {
            cullingSystem.Reset();

            geometryBuffer.reset();
            geometryUploadBuffer[0].reset();
            geometryUploadBuffer[1].reset();
            bindlessGeometryBuffer.reset();

            materialBuffer.reset();
            materialUploadBuffer[0].reset();
            materialUploadBuffer[1].reset();
            bindlessMaterialBuffer.reset();
        }

        void Init()override
        {
            InitSystems();
        }

        void InitSystems();

        void Uninit()override
        {
            for (auto system : systems)
                CJING_SAFE_DELETE(system);
            systems.clear();

            // TODO: unbind the callback
            // world.SetComponenetOnRemoved<MeshComponent>(nullptr);

            for (auto kvp : modelEntityMap)
            {
                Model* model = kvp.first;
                model->GetStateChangedCallback().Unbind<&RenderSceneImpl::OnModelStateChanged>(this);
            }
            modelEntityMap.clear();
        }

        void AddSystem(ISystem* system)
        {
            systems.push_back(system);
        }

        void Update(float dt, bool paused)override
        {
            UpdateSceneBuffers();

            // Update upload geometry buffer
            GPU::DeviceVulkan& device = *engine.GetWSI().GetDevice();
            if (geometryUploadBuffer[device.GetFrameIndex()])
                geometryMapped = (ShaderGeometry*)device.MapBuffer(*geometryUploadBuffer[device.GetFrameIndex()], GPU::MEMORY_ACCESS_WRITE_BIT);

            // Update systems
            for (auto system : systems)
                system->UpdateSystem();

            if (geometryMapped)
                device.UnmapBuffer(*geometryUploadBuffer[device.GetFrameIndex()], GPU::MEMORY_ACCESS_WRITE_BIT);

            // Update shader scene
            sceneCB.geometrybuffer = bindlessGeometryBuffer ? bindlessGeometryBuffer->GetIndex() : -1;
            sceneCB.materialbuffer = bindlessMaterialBuffer ? bindlessMaterialBuffer->GetIndex() : -1;
        }
        
        void Clear()override
        {
            modelEntityMap.clear();
        }

        World& GetWorld()override
        {
            return world;
        }

        IPlugin& GetPlugin()const override
        {
            return rendererPlugin;
        }

        ECS::EntityID CreateEntity(const char* name) override
        {
            return world.CreateEntity(name).entity;
        }

        void DestroyEntity(ECS::EntityID entity)override
        {
            world.DeleteEntity(entity);
        }

        CameraComponent* GetMainCamera()override
        {
            return &mainCamera;
        }

        void UpdateVisibility(struct Visibility& vis)
        {
            cullingSystem->Cull(vis, *this);
        }

        void AddMeshInstance(ECS::EntityID entity)
        {
            world.AddComponent<TransformComponent>(entity);
            world.AddComponent<MeshComponent>(entity);
        }

        ECS::EntityID CreateMeshInstance(const char* name)override
        {
            return world.CreateEntity(name)
                .With<TransformComponent>()
                .With<MeshComponent>()
                .entity;
        }
        
        void ForEachMeshes(std::function<void(ECS::EntityID, MeshComponent&)> func) override
        {
            if (!geometryBuffer || !materialBuffer)
                return;

            if (meshQuery.Valid())
                meshQuery.ForEach(func);
        }

        void SetModelResource(ECS::EntityID entity, ResPtr<Model> model)
        {
            ModelComponent* modelCmp = world.GetComponent<ModelComponent>(entity);
            if (modelCmp->model == model)
                return;

            if (modelCmp->model)
            {
                RemoveFromModelEntityMap(modelCmp->model.get(), entity);
                modelCmp->model.reset();
            }

            modelCmp->model = std::move(model);

            if (modelCmp->model)
            {
                AddToModelEntityMap(modelCmp->model.get(), entity);
                if (modelCmp->model->IsReady())
                    OnModelLoaded(modelCmp->model.get(), entity);
            }
        }

        void AddToModelEntityMap(Model* model, ECS::EntityID entity)
        {
            auto it = modelEntityMap.find(model);
            if (it != modelEntityMap.end()) 
            {
                modelEntityMap[model] = entity;
            }
            else 
            {
                modelEntityMap.insert({ model, entity });
                model->GetStateChangedCallback().Bind<&RenderSceneImpl::OnModelStateChanged>(this);
            }
        }

        void RemoveFromModelEntityMap(Model* model, ECS::EntityID entity)
        {
            auto it = modelEntityMap.find(model);
            if (it != modelEntityMap.end() && it->second == entity)
            {
                model->GetStateChangedCallback().Unbind<&RenderSceneImpl::OnModelStateChanged>(this);
                modelEntityMap.erase(it);
            }
        }
    
        void OnModelLoaded(Model* model, ECS::EntityID entity)
        {
            ASSERT(model->IsReady());

            ModelComponent* modelCmp = world.GetComponent<ModelComponent>(entity);
            modelCmp->mesh = &model->GetMesh(0);
            modelCmp->meshCount = model->GetMeshCount();

            for (int i = 0; i < modelCmp->meshCount; i++)
            {
                Mesh& mesh = modelCmp->mesh[i];
                auto meshEntity = CreateMeshInstance(mesh.name.c_str());
                MeshComponent* meshCmp = world.GetComponent<MeshComponent>(meshEntity);
                meshCmp->meshIndex = i;
                meshCmp->model = entity;
            }
        }

        void OnModelUnloaded(Model* model, ECS::EntityID entity)
        {
            ModelComponent* modelCmp = world.GetComponent<ModelComponent>(entity);
            modelCmp->mesh = nullptr;
            modelCmp->meshCount = 0;
        }

        ECS::EntityID CreateModel(const char* name)
        {
            return world.CreateEntity(name)
                .With<ModelComponent>()
                .entity;
        }

        void LoadModel(const char* name, const Path& path) override
        {
            ECS::EntityID entity = CreateModel(name);
            if (path.IsEmpty())
            {
                SetModelResource(entity, ResPtr<Model>());
            }
            else
            {
                ResPtr<Model> model = engine.GetResourceManager().LoadResourcePtr<Model>(path);
                SetModelResource(entity, std::move(model));
            }
        }

        void OnModelStateChanged(Resource::State oldState, Resource::State newState, Resource& resource)
        {
            Model* model = static_cast<Model*>(&resource);
            if (newState == Resource::State::READY)
            {
                auto it = modelEntityMap.find(model);
                if (it != modelEntityMap.end())
                    OnModelLoaded(model, it->second);
            }
            else if (oldState == Resource::State::READY)
            {
                auto it = modelEntityMap.find(model);
                if (it != modelEntityMap.end())
                    OnModelUnloaded(model, it->second);
            }
        }

        void UpdateSceneBuffers()
        {
            GPU::DeviceVulkan& device = *engine.GetWSI().GetDevice();

            // Create material buffer
            Array<Material*> materials;
            for (auto kvp : modelEntityMap)
            {
                auto model = kvp.first;
                for (int i = 0; i < model->GetMeshCount(); i++)
                {
                    auto& mesh = model->GetMesh(i);
                    for (auto& subset : mesh.subsets)
                    {
                        if (subset.material)
                            materials.push_back(subset.material.get());
                    }
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
            ShaderMaterial* materialMapped = nullptr;
            if (materialUploadBuffer[device.GetFrameIndex()])
                materialMapped = (ShaderMaterial*)device.MapBuffer(*materialUploadBuffer[device.GetFrameIndex()], GPU::MEMORY_ACCESS_WRITE_BIT);
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
            for (auto kvp : modelEntityMap)
            {
                auto model = kvp.first;
                for (int i = 0; i < model->GetMeshCount(); i++)
                {
                    Mesh& mesh = model->GetMesh(i);
                    mesh.geometryOffset = geometryArraySize;
                    geometryArraySize += mesh.subsets.size();
                }
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

            // Update material index
            for (auto kvp : modelEntityMap)
            {
                auto model = kvp.first;
                for (int i = 0; i < model->GetMeshCount(); i++)
                {
                    auto& mesh = model->GetMesh(i);
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
                    }
                }
            }
        }

        void UpdateRenderData(GPU::CommandList& cmd)
        {
            auto& device = cmd.GetDevice();
            if (geometryBuffer && geometryArraySize > 0)
            {
                auto uploadBuffer = geometryUploadBuffer[device.GetFrameIndex()];
                if (uploadBuffer)
                {
                    cmd.CopyBuffer(
                        *geometryBuffer, 
                        0,
                        *uploadBuffer,
                        0,
                        geometryArraySize * sizeof(ShaderGeometry)
                    );
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
                    cmd.CopyBuffer(
                        *materialBuffer, 
                        0,
                        *uploadBuffer,
                        0,
                        materialArraySize * sizeof(ShaderMaterial)
                    );
                    cmd.BufferBarrier(*materialBuffer,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_ACCESS_SHADER_READ_BIT);
                }
            }
        }

        const ShaderSceneCB& GetShaderScene()const override
        {
            return sceneCB;
        }
    };

    class ModelUpdateSystem : public ISystem
    {
    public:
        ModelUpdateSystem(RenderSceneImpl& scene) : ISystem(scene)
        {
            system = scene.GetWorld().CreateSystem<ModelComponent>()
                .ForEach([&](ECS::EntityID entity, ModelComponent& modelComp) {

                auto geometryMapped = scene.geometryMapped;
                if (!geometryMapped)
                    return;

                for (int i = 0; i < modelComp.meshCount; i++)
                {
                    Mesh& mesh = modelComp.mesh[i];

                    ShaderGeometry geometry;
                    geometry.vbPos = mesh.vbPos.srv->GetIndex();
                    geometry.vbNor = mesh.vbNor.srv->GetIndex();
                    geometry.vbUVs = mesh.vbUVs.srv->GetIndex();
                    geometry.ib = 0;

                    U32 subsetIndex = 0;
                    for (auto& subset : mesh.subsets)
                    {
                        memcpy(geometryMapped + mesh.geometryOffset + subsetIndex, &geometry, sizeof(ShaderGeometry));
                        subsetIndex++;
                    }
                }
            });
        }
    };

    class MeshUpdateSystem : public ISystem
    {
    public:
        MeshUpdateSystem(RenderSceneImpl& scene) : ISystem(scene)
        {
            system = scene.GetWorld().CreateSystem<MeshComponent>()
                .ForEach([&](ECS::EntityID entity, MeshComponent& meshComp) {

                auto geometryMapped = scene.geometryMapped;
                if (!geometryMapped)
                    return;

                if (meshComp.meshIndex < 0 || meshComp.model == ECS::INVALID_ENTITY)
                    return;

                AABB& aabb = meshComp.aabb;
                aabb = AABB();

                auto modelComp = scene.GetWorld().GetComponent<ModelComponent>(meshComp.model);
                if (modelComp != nullptr && modelComp->meshCount > meshComp.meshIndex)
                {
                    Mesh* mesh = &modelComp->mesh[meshComp.meshIndex];
                    aabb = mesh->aabb;
                }
            });
        }
    };

    void RenderSceneImpl::InitSystems()
    {
        AddSystem(CJING_NEW(ModelUpdateSystem)(*this));
        AddSystem(CJING_NEW(MeshUpdateSystem)(*this));
    }

    UniquePtr<RenderScene> RenderScene::CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world)
    {
        return CJING_MAKE_UNIQUE<RenderSceneImpl>(rendererPlugin, engine, world);
    }

    void RenderScene::Reflect(World* world)
    {
        Reflection::Builder builder = Reflection::BuildScene(world, "RenderScene");
        builder.Component<MeshComponent, &RenderScene::CreateMeshInstance, &RenderScene::DestroyEntity>("MeshInstance");
    }
}