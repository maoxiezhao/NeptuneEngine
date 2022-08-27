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

        VECTOR Eye = Vector3Transform(VectorSet(0, 0, 0, 1), mat);
        VECTOR At = Vector3Normalize(Vector3TransformNormal(XMVectorSet(0, 0, 1, 0), mat));
        VECTOR Up = Vector3Normalize(Vector3TransformNormal(XMVectorSet(0, 1, 0, 0), mat));

        MATRIX _V = MatrixLookToLH(Eye, At, Up);
        view = StoreFMat4x4(_V);
        rotationMat = StoreFMat3x3(MatrixInverse(_V));

        eye = StoreF32x3(Eye);
        at = StoreF32x3(At);
        up = StoreF32x3(Up);
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
        CameraComponent mainCamera;
        UniquePtr<CullingSystem> cullingSystem;
        Array<ECS::System> systems;
        ECS::Pipeline pipeline;

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

            world.SetComponenetOnRemoved<LoadModelComponent>([&](ECS::Entity entity, LoadModelComponent& model) {
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

        // Initialize all rendering systems
        void InitSystems();

        void Uninit()override
        {
            for (auto system : systems)
                system.Destroy();
            systems.clear();

            for (auto kvp : modelEntityMap)
            {
                Model* model = kvp.first;
                model->OnLoadedCallback.Unbind<&RenderSceneImpl::OnModelLoadedCallback>(this);
                model->OnUnloadedCallback.Unbind<&RenderSceneImpl::OnModelUnloadedCallback>(this);
            }
            modelEntityMap.clear();
        }

        ECS::Entity CreateEntity(const char* name) override
        {
            return world.CreateEntity(name);
        }

        void DestroyEntity(ECS::Entity entity)override
        {
            world.DeleteEntity(entity);
        }

        void AddSystem(const ECS::System& system)
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

        PickResult CastRayPick(const Ray& ray, U32 mask = ~0)override
        {
            PickResult ret;
            ForEachObjects([&](ECS::Entity entity, ObjectComponent& comp) 
            {
                const AABB& aabb = comp.aabb;
                if (!ray.Intersects(aabb))
                    return;

                if (comp.mesh == ECS::INVALID_ENTITY)
                    return;

                auto meshComp = comp.mesh.Get<MeshComponent>();
                if (meshComp == nullptr || !meshComp->model || !meshComp->model->IsReady())
                    return;

                // Transform ray to local object space
                const MATRIX objectMat = LoadFMat4x4(comp.worldMat);
                const MATRIX objectMatInverse = MatrixInverse(objectMat);
                const VECTOR rayOriginLocal = Vector3Transform(LoadF32x3(ray.origin), objectMatInverse);
                const XMVECTOR rayDirectionLocal = XMVector3Normalize(XMVector3TransformNormal(LoadF32x3(ray.direction), objectMatInverse));

                PickResult hit = meshComp->mesh->CastRayPick(rayOriginLocal, rayDirectionLocal, ray.tMin, ray.tMax);
                if (hit.isHit)
                {
                    // Calculate the distance in world space
                    const VECTOR posV = Vector3Transform(VectorAdd(rayOriginLocal, rayDirectionLocal * hit.distance), objectMat);
                    F32x3 pos = StoreF32x3(posV);
                    F32 distance = Distance(pos, ray.origin);

                    if (distance < ret.distance)
                    {
                        ret.isHit = true;
                        ret.distance = distance;
                        ret.entity = entity;
                        ret.position = pos;
                        ret.normal = StoreF32x3(Vector3Normalize(Vector3Transform(LoadF32x3(hit.normal), objectMat)));
                    }
                }
            });
            return ret;
        }

        void CreateComponent(ECS::Entity entity, ECS::EntityID compID) override
        {
            auto compMeta = Reflection::GetComponent(compID);
            if (compMeta == nullptr)
            {
                Logger::Error("Unregistered component type %d to create", compID);
                return;
            }

            compMeta->creator(this, entity);
        }

        CameraComponent* GetMainCamera()override
        {
            return &mainCamera;
        }

        void UpdateVisibility(struct Visibility& vis)
        {
            cullingSystem->Cull(vis, *this);
        }

        ECS::Entity CreateObject(ECS::Entity entity)override
        {
            if (entity == ECS::INVALID_ENTITY)
                entity = world.CreateEntity(nullptr);

            return entity.Add<TransformComponent>()
                .Add<ObjectComponent>();
        }

        bool IsSceneValid()const
        {
            return 
                instanceBuffer.IsValid() &&
                geometryBuffer.IsValid() &&
                materialBuffer.IsValid();
        }
        
        void ForEachObjects(std::function<void(ECS::Entity, ObjectComponent&)> func) override
        {
            if (!IsSceneValid())
                return;

            if (objectQuery.Valid())
                objectQuery.ForEach(func);
        }

        ECS::Entity CreateMesh(ECS::Entity entity) override
        {
            if (entity == ECS::INVALID_ENTITY)
                entity = world.CreateEntity(nullptr);

            return entity.Add<MeshComponent>();
        }

        ECS::Entity CreateMaterial(ECS::Entity entity) override
        {
            if (entity == ECS::INVALID_ENTITY)
                entity = world.CreateEntity(nullptr);

            return entity.Add<MaterialComponent>();
        }

        ECS::Entity CreatePointLight(ECS::Entity entity)override
        {
            return CreateLight(entity, LightComponent::LightType::POINT);
        }

        ECS::Entity CreateLight(
            ECS::Entity entity,
            LightComponent::LightType type = LightComponent::LightType::POINT,
            const F32x3 pos = F32x3(1.0f),
            const F32x3 color = F32x3(1.0f),
            F32 intensity = 1.0f,
            F32 range = 10.0f)override
        {
            if (entity == ECS::INVALID_ENTITY)
                entity = world.CreateEntity(nullptr);
     
            entity.Add<TransformComponent>()
                .Add<LightComponent>();

            TransformComponent* transform = entity.GetMut<TransformComponent>();
            transform->transform.Translate(pos);
            transform->transform.UpdateTransform();

            LightComponent* light = entity.GetMut<LightComponent>();
            light->aabb.CreateFromHalfWidth(pos, F32x3(range));
            light->intensity = intensity;
            light->range = range;
            light->color = color;
            light->type = type;

            return entity;
        }

        void LoadModelResource(ECS::Entity entity, ResPtr<Model> model)
        {
            LoadModelComponent* modelCmp = entity.GetMut<LoadModelComponent>();
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

        void AddToModelEntityMap(Model* model, ECS::Entity entity)
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

        void RemoveFromModelEntityMap(Model* model, ECS::Entity entity)
        {
            auto it = modelEntityMap.find(model);
            if (it != modelEntityMap.end() && it->second == entity)
            {
                model->OnLoadedCallback.Unbind<&RenderSceneImpl::OnModelLoadedCallback>(this);
                model->OnUnloadedCallback.Unbind<&RenderSceneImpl::OnModelUnloadedCallback>(this);
                modelEntityMap.erase(it);
            }
        }
    
        void OnModelLoaded(Model* model, ECS::Entity entity)
        {
            // TODO use system to process

            ASSERT(model->IsReady());

            const LoadModelComponent* modelCmp = entity.Get<LoadModelComponent>();
            for (int i = 0; i < model->GetMeshCount(); i++)
            {
                Mesh& mesh = model->GetMesh(i);
                auto meshEntity = CreateMesh(CreateEntity(mesh.name.c_str()));
                MeshComponent* meshCmp = meshEntity.GetMut<MeshComponent>();
                meshCmp->model = modelCmp->model;
                meshCmp->mesh = &mesh;

                for (auto& subset : mesh.subsets)
                {
                    if (subset.material)
                    {
                        char name[64];
                        CopyString(Span(name), Path::GetBaseName(subset.material->GetPath().c_str()));
                        auto materialEntity = CreateMaterial(CreateEntity(name));
                        MaterialComponent* material = materialEntity.GetMut<MaterialComponent>();
                        material->material = subset.material;
                        
                        subset.materialID = materialEntity;
                    }
                }

                auto objectEntity = CreateObject(CreateEntity(entity.GetName()));
                ObjectComponent* obj = objectEntity.GetMut<ObjectComponent>();
                obj->mesh = meshEntity;
            }

            RemoveFromModelEntityMap(model, entity);
            world.DeleteEntity(entity);
        }

        void OnModelUnloaded(Model* model, ECS::Entity entity)
        {
            world.DeleteEntity(entity);
        }

        void LoadModel(const char* name, const Path& path) override
        {
            ECS::Entity entity = world.CreateEntity(name).Add<LoadModelComponent>();
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
                objectQuery.ForEach([&](ECS::Entity entity, ObjectComponent& obj) {
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
                materialQuery.ForEach([&](ECS::Entity entity, MaterialComponent& mat) {
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
                meshQuery.ForEach([&](ECS::Entity entity, MeshComponent& meshComp) {
                    if (meshComp.mesh != nullptr)
                    {
                        meshComp.geometryOffset = geometryArraySize;
                        geometryArraySize += meshComp.mesh->subsets.size();
                    }
                });
            }
            geometryBuffer.UpdateBuffer(device, geometryArraySize);
            geometryMapped = geometryBuffer.Mapping(device);

            // Run systems
            if (pipeline)
                world.RunPipeline(pipeline);

            // Update shader scene
            sceneCB.instancebuffer = instanceBuffer.GetBindlessIndex();
            sceneCB.geometrybuffer = geometryBuffer.GetBindlessIndex();
            sceneCB.materialbuffer = materialBuffer.GetBindlessIndex();
        }
    };

    struct RenderingSystem {};

    ECS::System TransformUpdateSystem(RenderSceneImpl& scene)
    {
        return scene.GetWorld().CreateSystem<TransformComponent>()
            .Kind<RenderingSystem>()
            .MultiThread(true)
            .ForEach([&](ECS::Entity entity, TransformComponent& transComp) {
            transComp.transform.UpdateTransform();
        });
    }

    ECS::System MeshUpdateSystem(RenderSceneImpl& scene)
    {
        return scene.GetWorld().CreateSystem<MeshComponent>()
            .Kind<RenderingSystem>()
            .MultiThread(true)
            .ForEach([&](ECS::Entity entity, MeshComponent& meshComp) {

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

    ECS::System MaterialUpdateSystem(RenderSceneImpl& scene)
    {
        return scene.GetWorld().CreateSystem<MaterialComponent>()
            .Kind<RenderingSystem>()
            .MultiThread(true)
            .ForEach([&](ECS::Entity entity, MaterialComponent& materialComp) {

            auto materialMapped = scene.materialMapped;
            if (!materialMapped || !materialComp.material)
                return;

            materialComp.material->WriteShaderMaterial(materialMapped + materialComp.materialIndex);
         });
    }

    ECS::System ObjectUpdateSystem(RenderSceneImpl& scene)
    {
        return scene.GetWorld().CreateSystem<const TransformComponent, ObjectComponent>()
            .Kind<RenderingSystem>()
            .MultiThread(true)
            .ForEach([&](ECS::Entity entity, const TransformComponent& transform, ObjectComponent& objComp) {

            auto instanceMapped = scene.instanceMapped;
            if (!instanceMapped)
                return;

            AABB& aabb = objComp.aabb;
            aabb = AABB();

            if (objComp.mesh != ECS::INVALID_ENTITY)
            {
                auto meshComp = objComp.mesh.Get<MeshComponent>();
                ASSERT(meshComp->mesh != nullptr);

                MATRIX mat = LoadFMat4x4(transform.transform.world);
                aabb = meshComp->mesh->aabb.Transform(mat);
                objComp.center = StoreFMat4x4(meshComp->mesh->aabb.GetCenterAsMatrix() * mat).vec[3].xyz();

                ShaderMeshInstance inst;
                inst.init();
                inst.transform.Create(transform.transform.world);
                objComp.worldMat = transform.transform.world;

                memcpy(instanceMapped + objComp.index, &inst, sizeof(ShaderMeshInstance));
            }
        });
    }

    void RenderSceneImpl::InitSystems()
    {
        AddSystem(TransformUpdateSystem(*this));
        AddSystem(MeshUpdateSystem(*this));
        AddSystem(MaterialUpdateSystem(*this));
        AddSystem(ObjectUpdateSystem(*this));

        pipeline = world.CreatePipeline()
            .Term(ECS::EcsCompSystem)
            .Term<RenderingSystem>()
            .Build();
    }

    UniquePtr<RenderScene> RenderScene::CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world)
    {
        return CJING_MAKE_UNIQUE<RenderSceneImpl>(rendererPlugin, engine, world);
    }
}