#include "core\jobsystem\jobsystem.h"
#include "core\platform\platform.h"
#include "ecs\ecs\ecs.h"

using namespace VulkanTest;

struct PositionComponent
{
    COMPONENT(PositionComponent);
    float x = 0.0f;
    float y = 0.0f;
};

struct VelocityComponent
{
    COMPONENT(VelocityComponent);
    float x = 0.0f;
    float y = 0.0f;
};

int main()
{
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