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
        ECS::Query<ObjectComponent> objectQuery;
        ECS::Query<MeshComponent> meshQuery;
        ECS::Query<MaterialComponent> materialQuery;

        // Runtime rendering infos
        GPU::BufferPtr geometryBuffer;
        U32 geometryArraySize = 0;
        ShaderGeometry* geometryMapped = nullptr;
        GPU::BufferPtr geometryUploadBuffer[2];
        GPU::BindlessDescriptorPtr bindlessGeometryBuffer;

        GPU::BufferPtr materialBuffer;
        U32 materialArraySize = 0;
        ShaderMaterial* materialMapped = nullptr;
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

            objectQuery = world.CreateQuery<ObjectComponent>().Build();
            meshQuery = world.CreateQuery<MeshComponent>().Build();
            materialQuery = world.CreateQuery<MaterialComponent>().Build();

            world.SetComponenetOnRemoved<LoadModelComponent>([&](ECS::EntityID entity, LoadModelComponent& model) {
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
            GPU::DeviceVulkan& device = *engine.GetWSI().GetDevice();

            UpdateSceneBuffers();

            // Update upload material buffer
            if (materialUploadBuffer[device.GetFrameIndex()])
                materialMapped = (ShaderMaterial*)materialUploadBuffer[device.GetFrameIndex()]->GetAllcation().hostBase;
            
            // Update upload geometry buffer
            if (geometryUploadBuffer[device.GetFrameIndex()])
                geometryMapped = (ShaderGeometry*)geometryUploadBuffer[device.GetFrameIndex()]->GetAllcation().hostBase;

            // Update systems
            for (auto system : systems)
                system->UpdateSystem();

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

        ECS::EntityID CreateObject(const char* name)override
        {
            return world.CreateEntity(name)
                .With<TransformComponent>()
                .With<ObjectComponent>()
                .entity;
        }
        
        void ForEachObjects(std::function<void(ECS::EntityID, ObjectComponent&)> func) override
        {
            if (!geometryBuffer || !materialBuffer)
                return;

            if (objectQuery.Valid())
                objectQuery.ForEach(func);
        }

        ECS::EntityID CreateMesh(const char* name) override
        {
            return world.CreateEntity(name)
                .With<MeshComponent>()
                .entity;
        }

        ECS::EntityID CreateMaterial(const char* name) override
        {
            return world.CreateEntity(name)
                .With<MaterialComponent>()
                .entity;
        }

        void LoadModelResource(ECS::EntityID entity, ResPtr<Model> model)
        {
            LoadModelComponent* modelCmp = world.GetComponent<LoadModelComponent>(entity);
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

            LoadModelComponent* modelCmp = world.GetComponent<LoadModelComponent>(entity);
            for (int i = 0; i < model->GetMeshCount(); i++)
            {
                Mesh& mesh = model->GetMesh(i);
                auto meshEntity = CreateMesh(mesh.name.c_str());
                MeshComponent* meshCmp = world.GetComponent<MeshComponent>(meshEntity);
                meshCmp->model = modelCmp->model;
                meshCmp->mesh = &mesh;

                for (auto& subset : mesh.subsets)
                {
                    if (subset.material)
                    {
                        char name[64];
                        CopyString(Span(name), Path::GetBaseName(subset.material->GetPath().c_str()));
                        auto materialEntity = CreateMaterial(name);
                        MaterialComponent* material = world.GetComponent<MaterialComponent>(materialEntity);
                        material->material = subset.material;
                        
                        subset.materialID = materialEntity;
                    }
                }

                auto objectEntity = CreateObject(world.GetEntityName(entity));
                ObjectComponent* obj = world.GetComponent<ObjectComponent>(objectEntity);
                obj->mesh = meshEntity;
            }

            RemoveFromModelEntityMap(model, entity);
            world.DeleteEntity(entity);
        }

        void OnModelUnloaded(Model* model, ECS::EntityID entity)
        {
            world.DeleteEntity(entity);
        }

        void LoadModel(const char* name, const Path& path) override
        {
            ECS::EntityID entity = world.CreateEntity(name).With<LoadModelComponent>().entity;
            if (path.IsEmpty())
            {
                LoadModelResource(entity, ResPtr<Model>());
            }
            else
            {
                ResPtr<Model> model = engine.GetResourceManager().LoadResourcePtr<Model>(path);
                LoadModelResource(entity, std::move(model));
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
            materialArraySize = 0;
            if (materialQuery.Valid())
            {
                materialQuery.ForEach([&](ECS::EntityID entity, MaterialComponent& mat) {
                    mat.materialIndex = materialArraySize;
                    materialArraySize++;
                });
            }
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

            // Create geometry buffer
            geometryArraySize = 0;
            if (meshQuery.Valid())
            {
                meshQuery.ForEach([&](ECS::EntityID entity, MeshComponent& meshComp) {
                    if (meshComp.mesh != nullptr)
                    {
                        meshComp.geometryOffset = geometryArraySize;
                        geometryArraySize += meshComp.mesh->subsets.size();
                    }
                });
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

    class MaterialUpdateSystem : public ISystem
    {
    public:
        MaterialUpdateSystem(RenderSceneImpl& scene) : ISystem(scene)
        {
            system = scene.GetWorld().CreateSystem<MaterialComponent>()
                .ForEach([&](ECS::EntityID entity, MaterialComponent& materialComp) {

                auto materialMapped = scene.materialMapped;
                if (!materialMapped || !materialComp.material)
                    return;

                ShaderMaterial shaderMaterial;
                shaderMaterial.baseColor = materialComp.material->GetColor().ToFloat4();

                memcpy(materialMapped + materialComp.materialIndex, &shaderMaterial, sizeof(ShaderMaterial));
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
                if (!geometryMapped || !meshComp.mesh)
                    return;

                Mesh& mesh = *meshComp.mesh;
                ShaderGeometry geometry;
                geometry.vbPos = mesh.vbPos.srv->GetIndex();
                geometry.vbNor = mesh.vbNor.srv->GetIndex();
                geometry.vbUVs = mesh.vbUVs.srv->GetIndex();
                geometry.ib = 0;

                U32 subsetIndex = 0;
                for (auto& subset : mesh.subsets)
                {
                    memcpy(geometryMapped + meshComp.geometryOffset + subsetIndex, &geometry, sizeof(ShaderGeometry));
                    subsetIndex++;
                }
            });
        }
    };

    class ObjectUpdateSystem : public ISystem
    {
    public:
        ObjectUpdateSystem(RenderSceneImpl& scene) : ISystem(scene)
        {
            system = scene.GetWorld().CreateSystem<ObjectComponent>()
                .ForEach([&](ECS::EntityID entity, ObjectComponent& objComp) {

                if (objComp.mesh == ECS::INVALID_ENTITY)
                    return;

                AABB& aabb = objComp.aabb;
                aabb = AABB();

                auto meshComp = scene.GetWorld().GetComponent<MeshComponent>(objComp.mesh);
                if (meshComp != nullptr && meshComp->mesh != nullptr)
                {
                    aabb = meshComp->mesh->aabb;
                }
            });
        }
    };

    void RenderSceneImpl::InitSystems()
    {
        AddSystem(CJING_NEW(MaterialUpdateSystem)(*this));
        AddSystem(CJING_NEW(MeshUpdateSystem)(*this));
        AddSystem(CJING_NEW(ObjectUpdateSystem)(*this));
    }

    UniquePtr<RenderScene> RenderScene::CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world)
    {
        return CJING_MAKE_UNIQUE<RenderSceneImpl>(rendererPlugin, engine, world);
    }

    void RenderScene::Reflect(World* world)
    {
        Reflection::Builder builder = Reflection::BuildScene(world, "RenderScene");
        builder.Component<ObjectComponent, &RenderScene::CreateObject, &RenderScene::DestroyEntity>("Object");
    }
}