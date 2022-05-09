#include "RenderScene.h"
#include "renderer.h"
#include "culling.h"

namespace VulkanTest
{
    void MeshComponent::SetupRenderData()
    {
        GPU::DeviceVulkan* device = Renderer::GetDevice();

        GPU::BufferCreateInfo bufferInfo = {};
        bufferInfo.domain = GPU::BufferDomain::Device;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.size = vertexPos.size() * sizeof(F32x3);
        vboPos = device->CreateBuffer(bufferInfo, vertexPos.data());

        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufferInfo.size = indices.size() * sizeof(U32);
        ibo = device->CreateBuffer(bufferInfo, indices.data());
    }

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

    class MeshUpdateSystem : public ISystem
    {
    public:
        MeshUpdateSystem(RenderScene& scene) : ISystem(scene)
        {
            system = scene.GetWorld().CreateSystem<MeshComponent>()
                .ForEach([&](ECS::EntityID entity, MeshComponent& mesh) {
                    F32x3 minPoint = F32x3(std::numeric_limits<F32>::max(), std::numeric_limits<F32>::max(), std::numeric_limits<F32>::max());
                    F32x3 maxPoint = F32x3(std::numeric_limits<F32>::lowest(), std::numeric_limits<F32>::lowest(), std::numeric_limits<F32>::lowest());
                    for (auto point : mesh.vertexPos)
                    {
                        minPoint = Min(minPoint, point);
                        maxPoint = Min(maxPoint, point);
                    }
                    mesh.aabb = AABB(minPoint, maxPoint);
                });
        }
    };

    class RenderSceneImpl : public RenderScene
    {
    private:
        Engine& engine;
        World& world;
        RendererPlugin& rendererPlugin;
        std::vector<ISystem*> systems;
        CameraComponent mainCamera;
        UniquePtr<CullingSystem> cullingSystem;

        EntityMap<MeshComponent*> meshes;

    public:
        RenderSceneImpl(RendererPlugin& rendererPlugin_, Engine& engine_, World& world_) :
            rendererPlugin(rendererPlugin_),
            engine(engine_),
            world(world_)
        {
            cullingSystem = CullingSystem::Create();
        }

        virtual ~RenderSceneImpl()
        {
            cullingSystem.Reset();
        }

        void Init()override
        {
            AddSystem(CJING_NEW(MeshUpdateSystem)(*this));
        }

        void Uninit()override
        {
            for (auto system : systems)
                CJING_SAFE_DELETE(system);
            systems.clear();
        }

        void AddSystem(ISystem* system)
        {
            systems.push_back(system);
        }

        void Update(float dt, bool paused)override
        {
            for (auto system : systems)
                system->UpdateSystem();
        }
        
        void Clear()override
        {
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

        ECS::EntityID CreateMesh(const char* name)override
        {
            ECS::EntityID entity = world.CreateEntity(name)
                .With<MeshComponent>()
                .entity;
            meshes[entity] = world.GetComponent<MeshComponent>(entity);
            return entity;
        }

        EntityMap<MeshComponent*>& GetMeshes()override
        {
            return meshes;
        }
    };

    UniquePtr<RenderScene> RenderScene::CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world)
    {
        return CJING_MAKE_UNIQUE<RenderSceneImpl>(rendererPlugin, engine, world);
    }
}