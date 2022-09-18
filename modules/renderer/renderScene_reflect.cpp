#include "RenderScene.h"
#include "renderer.h"
#include "core\scene\reflection.h"

namespace VulkanTest
{
    void RenderScene::Reflect(World* world)
    {
        Reflection::Builder builder = Reflection::BuildScene(world, "RenderScene");
        builder.Component<ObjectComponent, &RenderScene::CreateObject, &RenderScene::DestroyEntity>("Object");
        builder.Component<MaterialComponent, &RenderScene::CreateMaterial, &RenderScene::DestroyEntity>("Material");
        builder.Component<MeshComponent, &RenderScene::CreateMesh, &RenderScene::DestroyEntity>("Mesh");
        builder.Component<LightComponent, &RenderScene::CreateDirectionLight, &RenderScene::DestroyEntity>("DirectionalLight")
            .VarProp<LightComponent, &LightComponent::color>("color").ColorAttribute()
            .VarProp<LightComponent, &LightComponent::intensity>("intensity");
        //builder.Component<LightComponent, &RenderScene::CreatePointLight, &RenderScene::DestroyEntity>("PointLight")
        //    .VarProp<LightComponent, &LightComponent::color>("color").ColorAttribute()
        //    .VarProp<LightComponent, &LightComponent::intensity>("intensity")
        //    .VarProp<LightComponent, &LightComponent::range>("range");
    }
}