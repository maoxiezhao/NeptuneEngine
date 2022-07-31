#include "RenderScene.h"
#include "renderer.h"
#include "core\scene\reflection.h"

namespace VulkanTest
{
    void RenderScene::Reflect(World* world)
    {
        Reflection::Builder builder = Reflection::BuildScene(world, "RenderScene");
        builder.Component<ObjectComponent, &RenderScene::CreateObject, &RenderScene::DestroyEntity>("Object");
    }
}