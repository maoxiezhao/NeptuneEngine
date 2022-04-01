#include "core\jobsystem\jobsystem.h"
#include "core\platform\platform.h"
#include "ecs\ecs\ecs.h"

using namespace VulkanTest;

struct PositionComponent
{
    float x = 0.0f;
    float y = 0.0f;
};

struct VelocityComponent
{
    float x = 0.0f;
    float y = 0.0f;
};

struct AAAAComponent
{
    int value;
    char c1;
    char c2;
    char c3;
    char c4;
    int value2;
};

struct BBBBComponent
{
    int value;
    int value1;
    int value2;

    int func()
    {
        return value2;
    }
};

int main()
{
    std::cout << sizeof(AAAAComponent) << std::endl;
    std::cout << sizeof(BBBBComponent) << std::endl;

    void* mem = malloc(sizeof(AAAAComponent));
    static_cast<AAAAComponent*>(mem)->value2 = 4;
    int value = static_cast<BBBBComponent*>(mem)->func();

    free(mem);

    std::unique_ptr<ECS::World> world = ECS::World::Create();
    for (int i = 0; i < 500; i++)
    {
        world->CreateEntity((std::string("A") + std::to_string(i)).c_str())
            .With<PositionComponent>()
            .With<VelocityComponent>();
    }

    ECS::EntityID system = world->CreateSystem<PositionComponent, VelocityComponent>()
    .ForEach([&](ECS::EntityID entity, PositionComponent& pos, VelocityComponent& vel)
        {
            std::cout << "Update system" << std::endl;
        }
    );
    world->RunSystem(system);
	return 0;
}