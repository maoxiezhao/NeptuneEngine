#include "RenderScene.h"
#include "renderer.h"
#include "core\scene\reflection.h"

namespace VulkanTest
{
    static bool isInitialized = false;
    void RenderScene::Reflect()
    {
        if (isInitialized == true)
            return;

        Reflection::Builder builder = Reflection::BuildScene("RenderScene");
        builder.Component<ObjectComponent, &RenderScene::CreateObject, &RenderScene::DestroyEntity>("Object");
        builder.Component<MaterialComponent, &RenderScene::CreateMaterial, &RenderScene::DestroyEntity>("Material");
        builder.Component<MeshComponent, &RenderScene::CreateMesh, &RenderScene::DestroyEntity>("Mesh");
        builder.Component<LightComponent, &RenderScene::CreateDirectionLight, &RenderScene::DestroyEntity>("Light")
            .VarProp<LightComponent, &LightComponent::color>("color").ColorAttribute()
            .VarProp<LightComponent, &LightComponent::intensity>("intensity")
            .VarProp<LightComponent, &LightComponent::range>("range")
            .VarProp<LightComponent, &LightComponent::innerConeAngle>("innerConeAngle")
            .VarProp<LightComponent, &LightComponent::outerConeAngle>("outerConeAngle");

        isInitialized = true;
    }
}