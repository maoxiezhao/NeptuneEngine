#include "RenderScene.h"
#include "renderer.h"
#include "culling.h"
#include "gpu\vulkan\wsi.h"
#include "core\scene\reflection.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
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
        WeatherComponent weather;

        // The count of instances
        U32 instanceArraySize = 0;

        // Component query
        ECS::Query<ObjectComponent> objectQuery;
        ECS::Query<MeshComponent> meshQuery;
        ECS::Query<MaterialComponent> materialQuery;
        ECS::Query<LightComponent> lightQuery;

        // Runtime rendering infos
        RenderSceneBuffer<ShaderMeshInstance> instanceBuffer;
        RenderSceneBuffer<ShaderGeometry> geometryBuffer;
        RenderSceneBuffer<ShaderMaterial> materialBuffer;

        // Meshlets:
        GPU::BufferPtr meshletBuffer;
        GPU::BindlessDescriptorPtr meshletBufferBindless;
        volatile I32 meshletAllocator = 0;

        ShaderMeshInstance* instanceMapped = nullptr;
        ShaderMaterial* materialMapped = nullptr;
        ShaderGeometry* geometryMapped = nullptr;

        ShaderSceneCB sceneCB;

        std::vector<ECS::Entity> toLoadModels;

    public:
        RenderSceneImpl(RendererPlugin& rendererPlugin_, Engine& engine_, World& world_) :
            rendererPlugin(rendererPlugin_),
            engine(engine_),
            world(world_),
            instanceBuffer("InstanceBuffer"),
            geometryBuffer("GeometryBuffer"),
            materialBuffer("MaterialBuffer")
        {
            cullingSystem = CullingSystem::Create();

            objectQuery = world.CreateQuery<ObjectComponent>().Build();
            meshQuery = world.CreateQuery<MeshComponent>().Build();
            materialQuery = world.CreateQuery<MaterialComponent>().Build();
            lightQuery = world.CreateQuery<LightComponent>().Build();

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
            if (!cullingSystem->Initialize(*this))
            {
                Logger::Error("Faied to initialize culling system.");
                return;
            }

            InitSystems();
        }

        // Initialize all rendering systems
        void InitSystems();

        void Uninit()override
        {
            for (auto system : systems)
                system.Destroy();
            systems.clear();
            pipeline.Destroy();

            for (auto kvp : modelEntityMap)
            {
                Model* model = kvp.first;
                model->OnLoadedCallback.Unbind<&RenderSceneImpl::OnModelLoadedCallback>(this);
                model->OnUnloadedCallback.Unbind<&RenderSceneImpl::OnModelUnloadedCallback>(this);
            }
            modelEntityMap.clear();

            cullingSystem->Uninitialize();
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

                auto& mesheInfos = meshComp->meshes[0];
                for (auto& meshInfo : mesheInfos)
                {
                    PickResult hit = meshInfo.mesh->CastRayPick(rayOriginLocal, rayDirectionLocal, ray.tMin, ray.tMax);
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
                }
            });
            return ret;
        }

        void CreateComponent(ECS::Entity entity, ComponentType compType) override
        {
            auto compMeta = Reflection::GetComponent(compType);
            if (compMeta == nullptr)
            {
                Logger::Error("Unregistered component type %d to create", compType.index);
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

        ECS::Entity CreateDirectionLight(ECS::Entity entity) override
        {
            return CreateLight(entity, LightComponent::LightType::DIRECTIONAL);
        }

        ECS::Entity CreateSpotLight(ECS::Entity entity) override
        {
            return CreateLight(entity, LightComponent::LightType::SPOT);
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

        void ForEachLights(std::function<void(ECS::Entity, LightComponent&)> func) override
        {
            if (!IsSceneValid())
                return;

            if (lightQuery.Valid())
                lightQuery.ForEach(func);
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
            ASSERT(model->IsLoaded());     

            // Materials
            auto materialEntity = CreateMaterial(entity);
            MaterialComponent* materialCmp = materialEntity.GetMut<MaterialComponent>();
            const auto& materials = model->GetMaterials();
            for (const auto& material : materials)
                materialCmp->materials.push_back(material.material);

            // Meshes
            auto meshEntity = CreateMesh(entity);
            auto meshCmp = meshEntity.GetMut<MeshComponent>();
            meshCmp->model = model;
            meshCmp->lodsCount = model->GetLODsCount();
            meshCmp->meshCount = 0;

            for (I32 lodIndex = 0; lodIndex < meshCmp->lodsCount; lodIndex++)
            {
                auto& meshes = model->GetModelLOD(lodIndex)->GetMeshes();
                for (int meshIndex = 0; meshIndex < meshes.size(); meshIndex++)
                {
                    Mesh& mesh = meshes[meshIndex];
                    MeshComponent::MeshInfo info = {};
                    info.aabb = mesh.aabb;
                    info.mesh = &mesh;
                    info.material = materialEntity;
                    meshCmp->meshes[lodIndex].push_back(info);
                    meshCmp->meshCount++;
                }
            }

            // ObjectComponent
            auto objectEntity = CreateObject(entity);
            ObjectComponent* obj = objectEntity.GetMut<ObjectComponent>();
            obj->mesh = meshEntity;
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
                ResPtr<Model> model = ResourceManager::LoadResource<Model>(path);
                LoadModelResource(entity, std::move(model));
            }
        }

        void OnModelLoadedCallback(Resource* resource)
        {
            Model* model = static_cast<Model*>(resource);
            auto it = modelEntityMap.find(model);
            if (it != modelEntityMap.end())
                toLoadModels.push_back(it->second);
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

            // Update scene common buffers
            instanceBuffer.UploadBuffer(device, cmd);
            geometryBuffer.UploadBuffer(device, cmd);
            materialBuffer.UploadBuffer(device, cmd);

            // Update meshlet buffer
            if (instanceArraySize > 0 && meshletBuffer)
            {
                Renderer::BindFrameCB(cmd);
                cmd.BeginEvent("Meshlet prepare");
                cmd.BufferBarrier(*meshletBuffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                cmd.SetStorageBuffer(0, 0, *meshletBuffer);
                cmd.SetProgram(Renderer::GetShader(SHADERTYPE_MESHLET)->GetCS("CS"));
                cmd.Dispatch(instanceArraySize, 1, 1);
                cmd.BufferBarrier(*meshletBuffer, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
                cmd.EndEvent();
            }
        }

        const ShaderSceneCB& GetShaderScene()const override
        {
            return sceneCB;
        }

        void Update(float dt, bool paused)override
        {
            PROFILE_FUNCTION();
            GPU::DeviceVulkan& device = *engine.GetWSI().GetDevice();

            // Process to load models
            for (auto entity : toLoadModels)
            {
                const LoadModelComponent* loadComp = entity.Get<LoadModelComponent>();
                if (loadComp->model)
                    OnModelLoaded(loadComp->model.get(), entity);
            }
            toLoadModels.clear();

            // Update instance buffer
            instanceArraySize = 0;
            if (objectQuery.Valid())
            {
                objectQuery.ForEach([&](ECS::Entity entity, ObjectComponent& obj) {
                    obj.instanceOffset = instanceArraySize;
                    auto meshComp = obj.mesh.Get<MeshComponent>();
                    if (meshComp != nullptr)
                        instanceArraySize += meshComp->meshCount;
                    
                });
            }
            instanceBuffer.UpdateBuffer(device, instanceArraySize);
            instanceMapped = instanceBuffer.Mapping(device);
            
            // Update material buffer
            U32 materialArraySize = 0;
            if (materialQuery.Valid())
            {
                materialQuery.ForEach([&](ECS::Entity entity, MaterialComponent& mat) {
                    mat.materialOffset = materialArraySize;
                    materialArraySize += mat.materials.size();
                });
            }
            materialBuffer.UpdateBuffer(device, materialArraySize);
            materialMapped = materialBuffer.Mapping(device);

            // Update geometry buffer
            U32 geometryArraySize = 0;
            if (meshQuery.Valid())
            {
                meshQuery.ForEach([&](ECS::Entity entity, MeshComponent& meshComp) 
                {
                    if (!meshComp.model || !meshComp.model->IsReady())
                        return;

                    for (I32 lodIndex = 0; lodIndex < meshComp.lodsCount; lodIndex++)
                    {
                        for (auto& meshInfo : meshComp.meshes[lodIndex])
                        {
                            meshInfo.geometryOffset = geometryArraySize;
                            geometryArraySize += meshInfo.mesh->subsets.size();
                        }
                    }
                });
            }
            geometryBuffer.UpdateBuffer(device, geometryArraySize);
            geometryMapped = geometryBuffer.Mapping(device);

            // Reset meshlet count
            AtomicStore(&meshletAllocator, 0);

            // Run systems
            if (pipeline)
                world.RunPipeline(pipeline);

            // Create meshlet buffer
            I32 meshletCount = AtomicRead(&meshletAllocator);
            U32 bufferSize = meshletCount * sizeof(ShaderMeshlet);
            if (bufferSize > 0 && (!meshletBuffer || meshletBuffer->GetCreateInfo().size < bufferSize))
            {
                GPU::BufferCreateInfo info = {};
                info.domain = GPU::BufferDomain::Device;
                info.size = bufferSize * 2;
                info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                meshletBuffer = device.CreateBuffer(info, nullptr);
                device.SetName(*meshletBuffer, "meshletBuffer");
                meshletBufferBindless = device.CreateBindlessStroageBuffer(*meshletBuffer);
            }

            // Update shader scene
            sceneCB.instancebuffer = instanceBuffer.GetBindlessIndex();
            sceneCB.geometrybuffer = geometryBuffer.GetBindlessIndex();
            sceneCB.materialbuffer = materialBuffer.GetBindlessIndex();
            sceneCB.meshletBuffer = meshletBufferBindless ? meshletBufferBindless->GetIndex() : -1;

            sceneCB.weather.ambientColor = weather.ambientColor;
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
            if (!geometryMapped)
                return;

            if (!meshComp.model || !meshComp.model->IsReady())
                return;

            auto lodsCount = meshComp.model->GetLODsCount();
            for (int lodIndex = 0; lodIndex < lodsCount; lodIndex++)
            {
                for (auto& meshInfo : meshComp.meshes[lodIndex])
                {
                    // Reset meshlet count
                    meshInfo.meshletCount = 0;

                    auto mesh = meshInfo.mesh;
                    ShaderGeometry geometry;
                    geometry.vbPosNor = mesh->vbPosNor.srv->GetIndex();
                    geometry.vbUVs = mesh->vbUVs.srv ? mesh->vbUVs.srv->GetIndex() : -1;
                    geometry.vbTan = mesh->vbTan.srv ? mesh->vbTan.srv->GetIndex() : -1;
                    geometry.ib = mesh->ib.srv->GetIndex();

                    U32 subsetIndex = 0;
                    for (auto& subset : mesh->subsets)
                    {
                        geometry.indexOffset = subset.indexOffset;
                        geometry.meshletOffset = meshInfo.meshletCount;
                        geometry.meshletCount = TriangleCountToMeshletCount(subset.indexCount);
                        meshInfo.meshletCount += geometry.meshletCount;

                        memcpy(geometryMapped + meshInfo.geometryOffset + subsetIndex, &geometry, sizeof(ShaderGeometry));
                        subsetIndex++;
                    }
                }
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
            if (!materialMapped || materialComp.materials.empty())
                return;

            for (int i = 0; i < materialComp.materials.size(); i++)
            {
                auto& material = materialComp.materials[i];
                if (material->IsReady())
                    material->WriteShaderMaterial(materialMapped + materialComp.materialOffset + i);
            }
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

            const MeshComponent* meshComp = nullptr;
            AABB& aabb = objComp.aabb;
            aabb = AABB();
            if (objComp.mesh != ECS::INVALID_ENTITY)
            {
                meshComp = objComp.mesh.Get<MeshComponent>();
                if (meshComp == nullptr)
                    return;

                I32 instOffset = objComp.instanceOffset;
                MATRIX mat = LoadFMat4x4(transform.transform.world);
                for (int lodIndex = 0; lodIndex < meshComp->lodsCount; lodIndex++)
                {
                    for (auto& meshInfo : meshComp->meshes[lodIndex])
                    {
                        auto mesh = meshInfo.mesh;
                        if (lodIndex == 0)
                            aabb.Merge(meshInfo.aabb.Transform(mat));

                        // Setup shader mesh instance
                        ShaderMeshInstance inst;
                        inst.init();
                        inst.uid = uint((ECS::EntityID)objComp.mesh);   // Need use u64?
                        inst.geometryOffset = meshInfo.geometryOffset;
                        inst.geometryCount = mesh->subsets.size();   // TODO select the subset for target lod
                        inst.meshletOffset = AtomicAdd(&scene.meshletAllocator, meshInfo.meshletCount) - meshInfo.meshletCount;

                        // Set world transform
                        inst.transform.Create(transform.transform.world);

                        // Inverse transpose world mat
                        MATRIX worldMatInvTranspose = MatrixTranspose(MatrixInverse(mat));
                        inst.transformInvTranspose.Create(StoreFMat4x4(worldMatInvTranspose));
                        memcpy(instanceMapped + instOffset, &inst, sizeof(ShaderMeshInstance));

                        instOffset++;
                    }
                }
      
                FMat4x4 meshMatrix = StoreFMat4x4(aabb.GetAsMatrix() * mat);
                objComp.center = meshMatrix.vec[3].xyz();
            }

            // objComp.center = aabb.GetCenter();
            objComp.radius = aabb.GetRadius();
            objComp.worldMat = transform.transform.world;

            // Calculate LOD
            if (meshComp != nullptr)
            {
                CameraComponent* camera = scene.GetMainCamera();
                objComp.lodIndex = meshComp->model->CalculateModelLOD(camera->eye, objComp.center, objComp.radius);
            }
            else
            {
                objComp.lodIndex = 0;
            }
        });
    }

    ECS::System LightUpdateSystem(RenderSceneImpl& scene)
    {
        return scene.GetWorld().CreateSystem<const TransformComponent, LightComponent>()
            .Kind<RenderingSystem>()
            .MultiThread(true)
            .ForEach([&](ECS::Entity entity, const TransformComponent& transform, LightComponent& light) {

            MATRIX worldMat = transform.transform.GetMatrix();
            VECTOR S, R, T;
            XMMatrixDecompose(&S, &R, &T, worldMat);

            light.position = StoreF32x3(T);
            light.rotation = StoreF32x4(R);
            light.scale = StoreF32x3(S);
            light.direction = StoreF32x3(Vector3TransformNormal(VectorSet(0, 1, 0, 0), worldMat));  // Up is Positive direction
          
            switch (light.type)
            {
            case LightComponent::DIRECTIONAL:
                light.aabb.FromHalfWidth(F32x3(0.0f), F32x3(FLT_MAX));
                break;
            case LightComponent::POINT:
                light.aabb.FromHalfWidth(light.position, F32x3(light.range));
                break;
            case LightComponent::SPOT:
                light.aabb.FromHalfWidth(light.position, F32x3(light.range));
                break;
            default:
                break;
            }
        });
    }

    void RenderSceneImpl::InitSystems()
    {
        AddSystem(TransformUpdateSystem(*this));
        AddSystem(MeshUpdateSystem(*this));
        AddSystem(MaterialUpdateSystem(*this));
        AddSystem(ObjectUpdateSystem(*this));
        AddSystem(LightUpdateSystem(*this));

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