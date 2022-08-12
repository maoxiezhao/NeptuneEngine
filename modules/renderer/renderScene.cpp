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

    template<typename T>
    struct RenderSceneBuffer
    {
        String name;
        String uploadName;

        GPU::BufferPtr buffer;
        U32 arraySize = 0;
        GPU::BufferPtr uploadBuffers[2];
        GPU::BindlessDescriptorPtr bindless;

        RenderSceneBuffer(const char* name_) :
            name(name_),
            uploadName(name_)
        {
            uploadName += "Upload";
        }

        bool IsValid()const
        {
            return buffer && arraySize > 0;
        }

        void UpdateBuffer(GPU::DeviceVulkan& device, U32 arraySize_)
        {
            if (arraySize_ == arraySize)
                return;

            arraySize = arraySize_;
            U32 bufferSize = arraySize * sizeof(T);
            if (arraySize > 0 && (!buffer || buffer->GetCreateInfo().size < bufferSize))
            {
                GPU::BufferCreateInfo info = {};
                info.domain = GPU::BufferDomain::Device;
                info.size = bufferSize * 2;
                info.usage = 
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                buffer = device.CreateBuffer(info, nullptr);
                device.SetName(*buffer, name.c_str());

                info.domain = GPU::BufferDomain::LinkedDeviceHost;
                info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

                for (int i = 0; i < ARRAYSIZE(uploadBuffers); i++)
                {
                    uploadBuffers[i] = device.CreateBuffer(info, nullptr);
                    device.SetName(*uploadBuffers[i], uploadName.c_str());
                }

                bindless = device.CreateBindlessStroageBuffer(*buffer, 0, buffer->GetCreateInfo().size);
            }
        }

        void UploadBuffer(GPU::DeviceVulkan& device, GPU::CommandList& cmd)
        {
            if (buffer && arraySize > 0)
            {
                auto uploadBuffer = uploadBuffers[device.GetFrameIndex()];
                if (uploadBuffer)
                {
                    cmd.CopyBuffer(
                        *buffer,
                        0,
                        *uploadBuffer,
                        0,
                        arraySize * sizeof(T)
                    );
                    cmd.BufferBarrier(*buffer,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_ACCESS_SHADER_READ_BIT);
                }
            }
        }

        T* Mapping(GPU::DeviceVulkan& device)
        {
            if (uploadBuffers[device.GetFrameIndex()])
                return (T*)uploadBuffers[device.GetFrameIndex()]->GetAllcation().hostBase;
            return nullptr;
        }

        I32 GetBindlessIndex()
        {
            return bindless ? bindless->GetIndex() : -1;
        }

        void Reset()
        {
            buffer.reset();
            uploadBuffers[0].reset();
            uploadBuffers[1].reset();
            bindless.reset();
        }
    };

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
        RenderSceneBuffer<ShaderMeshInstance> instanceBuffer;
        RenderSceneBuffer<ShaderGeometry> geometryBuffer;
        RenderSceneBuffer<ShaderMaterial> materialBuffer;

        ShaderMeshInstance* instanceMapped = nullptr;
        ShaderMaterial* materialMapped = nullptr;
        ShaderGeometry* geometryMapped = nullptr;

        ShaderSceneCB sceneCB;

    public:
        RenderSceneImpl(RendererPlugin& rendererPlugin_, Engine& engine_, World& world_) :
            rendererPlugin(rendererPlugin_),
            engine(engine_),
            world(world_),
            instanceBuffer("InstanceBuffer"),
            geometryBuffer("GeometryBuffer"),
            materialBuffer("MaterialBuffer")
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

            instanceBuffer.Reset();
            geometryBuffer.Reset();
            materialBuffer.Reset();
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

            // world.SetComponenetOnRemoved<MeshComponent>(nullptr);

            for (auto kvp : modelEntityMap)
            {
                Model* model = kvp.first;
                model->OnLoadedCallback.Unbind<&RenderSceneImpl::OnModelLoadedCallback>(this);
                model->OnUnloadedCallback.Unbind<&RenderSceneImpl::OnModelUnloadedCallback>(this);
            }
            modelEntityMap.clear();
        }

        void AddSystem(ISystem* system)
        {
            systems.push_back(system);
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

        bool IsSceneValid()const
        {
            return 
                instanceBuffer.IsValid() &&
                geometryBuffer.IsValid() &&
                materialBuffer.IsValid();
        }
        
        void ForEachObjects(std::function<void(ECS::EntityID, ObjectComponent&)> func) override
        {
            if (!IsSceneValid())
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
                model->OnLoadedCallback.Bind<&RenderSceneImpl::OnModelLoadedCallback>(this);
                model->OnUnloadedCallback.Bind<&RenderSceneImpl::OnModelUnloadedCallback>(this);
            }
        }

        void RemoveFromModelEntityMap(Model* model, ECS::EntityID entity)
        {
            auto it = modelEntityMap.find(model);
            if (it != modelEntityMap.end() && it->second == entity)
            {
                model->OnLoadedCallback.Unbind<&RenderSceneImpl::OnModelLoadedCallback>(this);
                model->OnUnloadedCallback.Unbind<&RenderSceneImpl::OnModelUnloadedCallback>(this);
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

        void OnModelLoadedCallback(Resource* resource)
        {
            Model* model = static_cast<Model*>(resource);
            auto it = modelEntityMap.find(model);
            if (it != modelEntityMap.end())
                OnModelLoaded(model, it->second);
        }

        void OnModelUnloadedCallback(Resource* resource)
        {
            Model* model = static_cast<Model*>(resource);
            auto it = modelEntityMap.find(model);
            if (it != modelEntityMap.end())
                OnModelUnloaded(model, it->second);
        }

        void UpdateRenderData(GPU::CommandList& cmd)
        {
            auto& device = cmd.GetDevice();
            instanceBuffer.UploadBuffer(device, cmd);
            geometryBuffer.UploadBuffer(device, cmd);
            materialBuffer.UploadBuffer(device, cmd);
        }

        const ShaderSceneCB& GetShaderScene()const override
        {
            return sceneCB;
        }

        void Update(float dt, bool paused)override
        {
            GPU::DeviceVulkan& device = *engine.GetWSI().GetDevice();

            // Update scene buffers

            // Update instance buffer
            U32 instanceArraySize = 0;
            if (objectQuery.Valid())
            {
                objectQuery.ForEach([&](ECS::EntityID entity, ObjectComponent& obj) {
                    obj.index = instanceArraySize;
                    instanceArraySize++;
                });
            }
            instanceBuffer.UpdateBuffer(device, instanceArraySize);
            instanceMapped = instanceBuffer.Mapping(device);
            
            // Update material buffer
            U32 materialArraySize = 0;
            if (materialQuery.Valid())
            {
                materialQuery.ForEach([&](ECS::EntityID entity, MaterialComponent& mat) {
                    mat.materialIndex = materialArraySize;
                    materialArraySize++;
                });
            }
            materialBuffer.UpdateBuffer(device, materialArraySize);
            materialMapped = materialBuffer.Mapping(device);

            // Update geometry buffer
            U32 geometryArraySize = 0;
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
            geometryBuffer.UpdateBuffer(device, geometryArraySize);
            geometryMapped = geometryBuffer.Mapping(device);

            // Update systems
            for (auto system : systems)
                system->UpdateSystem();

            // Update shader scene
            sceneCB.instancebuffer = instanceBuffer.GetBindlessIndex();
            sceneCB.geometrybuffer = geometryBuffer.GetBindlessIndex();
            sceneCB.materialbuffer = materialBuffer.GetBindlessIndex();
        }
    };

    class TransformUpdateSystem : public ISystem
    {
    public:
        TransformUpdateSystem(RenderSceneImpl& scene) : ISystem(scene)
        {
            system = scene.GetWorld().CreateSystem<TransformComponent>()
                .ForEach([&](ECS::EntityID entity, TransformComponent& transComp) {
                transComp.transform.UpdateTransform();
            });
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

                const auto& materialInfo = materialComp.material->GetParams();

                ShaderMaterial shaderMaterial;
                shaderMaterial.baseColor = materialInfo.color.ToFloat4();

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

                auto instanceMapped = scene.instanceMapped;
                if (!instanceMapped)
                    return;

                AABB& aabb = objComp.aabb;
                aabb = AABB();

                if (objComp.mesh != ECS::INVALID_ENTITY)
                {
                    auto transform = scene.GetComponent<TransformComponent>(entity);
                    auto meshComp = scene.GetComponent<MeshComponent>(objComp.mesh);
                    ASSERT(meshComp->mesh != nullptr);
                 
                    MATRIX mat = LoadFMat4x4(transform->transform.world);
                    aabb = meshComp->mesh->aabb.Transform(mat);
                    objComp.center = StoreFMat4x4(meshComp->mesh->aabb.GetCenterAsMatrix() * mat).vec[3].xyz();
                    
                    ShaderMeshInstance inst;
                    inst.init();
                    inst.transform.Create(transform->transform.world);

                    memcpy(instanceMapped + objComp.index, &inst, sizeof(ShaderMeshInstance));
                }
            });
        }
    };

    void RenderSceneImpl::InitSystems()
    {
        AddSystem(CJING_NEW(TransformUpdateSystem)(*this));
        AddSystem(CJING_NEW(MaterialUpdateSystem)(*this));
        AddSystem(CJING_NEW(MeshUpdateSystem)(*this));
        AddSystem(CJING_NEW(ObjectUpdateSystem)(*this));
    }

    UniquePtr<RenderScene> RenderScene::CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world)
    {
        return CJING_MAKE_UNIQUE<RenderSceneImpl>(rendererPlugin, engine, world);
    }
}