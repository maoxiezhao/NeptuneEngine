#include "RenderScene.h"
#include "renderer.h"
#include "culling.h"
#include "gpu\vulkan\wsi.h"

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
            cullingSystem = CullingSystem::Create();

            meshQuery = world.CreateQuery<MeshComponent>().Build();
            world.SetComponenetOnRemoved<MeshComponent>([&](ECS::EntityID entity, MeshComponent& mesh) {
                if (mesh.model)
                    modelEntityMap.erase(mesh.model.get());
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

        CameraComponent* GetMainCamera()override
        {
            return &mainCamera;
        }

        void UpdateVisibility(struct Visibility& vis)
        {
            cullingSystem->Cull(vis, *this);
        }

        ECS::EntityID CreateMeshInstance(const char* name, const Path& path)override
        {
            ECS::EntityID entity = world.CreateEntity(name)
                .With<TransformComponent>()
                .With<MeshComponent>()
                .entity;
            MeshComponent* mesh = world.GetComponent<MeshComponent>(entity);
            SetMeshInstance(entity, path);
            return entity;
        }
        
        void ForEachMeshes(std::function<void(ECS::EntityID, MeshComponent&)> func) override
        {
            if (!geometryBuffer || !materialBuffer)
                return;

            if (meshQuery.Valid())
                meshQuery.ForEach(func);
        }

        void SetMeshInstance(ECS::EntityID entity, const Path& path)
        {
            if (path.IsEmpty())
            {
                SetMeshInstance(entity, ResPtr<Model>());
            }
            else
            {
                ResPtr<Model> model = engine.GetResourceManager().LoadResourcePtr<Model>(path);
                SetMeshInstance(entity, std::move(model));
            }
        }

        void SetMeshInstance(ECS::EntityID entity, ResPtr<Model> model)
        {
            MeshComponent* mesh = world.GetComponent<MeshComponent>(entity);
            if (mesh->model == model)
                return;

            if (mesh->model)
            {
                RemoveFromModelEntityMap(mesh->model.get(), entity);
                mesh->model.reset();
            }

            mesh->model = std::move(model);

            if (mesh->model)
            {
                AddToModelEntityMap(mesh->model.get(), entity);
                if (mesh->model->IsReady())
                    OnModelLoaded(mesh->model.get(), entity);
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

            MeshComponent* mesh = world.GetComponent<MeshComponent>(entity);
            mesh->mesh = &model->GetMesh(0);
            mesh->meshCount = model->GetMeshCount();
        }

        void OnModelUnloaded(Model* model, ECS::EntityID entity)
        {
            MeshComponent* mesh = world.GetComponent<MeshComponent>(entity);
            mesh->mesh = nullptr;
            mesh->meshCount = 0;
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
                    cmd.CopyBuffer(*geometryBuffer, *uploadBuffer);
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
                    cmd.CopyBuffer(*materialBuffer, *uploadBuffer);
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

                for (int i = 0; i < meshComp.meshCount; i++)
                {
                    Mesh& mesh = meshComp.mesh[i];

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

    void RenderSceneImpl::InitSystems()
    {
        AddSystem(CJING_NEW(MeshUpdateSystem)(*this));
    }

    UniquePtr<RenderScene> RenderScene::CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world)
    {
        return CJING_MAKE_UNIQUE<RenderSceneImpl>(rendererPlugin, engine, world);
    }

    void RenderScene::Reflect()
    {
    }
}